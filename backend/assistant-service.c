/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "service-version.h"
#include "../platform/sdk-spawn.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
extern int ps5_install_runtime_image(void);
extern int ps5_install_runtime_tools(void);
#ifndef ASSISTANT_RPC_PORT
#define ASSISTANT_RPC_PORT 49322
#endif
#ifndef ASSISTANT_ADMIN_PORT
#define ASSISTANT_ADMIN_PORT 49323
#endif
/* One native UI and one resident companion can own independent engines. */
#define SESSION_LIMIT 2
#define CONTROL_LIMIT 4
struct queue {unsigned char bytes[65536];size_t begin,end;};
struct session {int client,io;pid_t child;struct queue *input,*output;int eof;};
struct control_client {int fd;char bytes[1024];size_t used;uint64_t deadline;};
static uint64_t now_ms(void){
    struct timespec value;if(clock_gettime(CLOCK_MONOTONIC,&value))return 0;
    return (uint64_t)value.tv_sec*1000+(uint64_t)value.tv_nsec/1000000;
}
static int nonblocking(int fd){
    int flags=fcntl(fd,F_GETFL);return flags<0?-1:fcntl(fd,F_SETFL,flags|O_NONBLOCK);
}
static void terminate(pid_t child){
    if(child<=0)return;
    int status;pid_t result;
    do {result=waitpid(child,&status,WNOHANG);}while(result<0 && errno==EINTR);
    if(result!=0)return; /* Never signal a PID that has already been reaped. */
    kill(-child,SIGTERM);
    for(unsigned attempts=0;attempts<20;++attempts){
        do {result=waitpid(child,&status,WNOHANG);}while(result<0 && errno==EINTR);
        if(result!=0)return;
        usleep(50000);
    }
    kill(-child,SIGKILL);
    while(waitpid(child,&status,0)<0 && errno==EINTR){}
}
static void destroy(struct session *s){
    if(s->client>=0)close(s->client);
    if(s->io>=0)close(s->io);
    terminate(s->child);free(s->input);free(s->output);
    *s=(struct session){.client=-1,.io=-1};
}
static int start(struct session *s,int server,int admin,struct session *sessions,struct control_client *controls){
    int io[2];if(socketpair(AF_UNIX,SOCK_STREAM,0,io))return -1;
    int errors=open("/dev/null",O_WRONLY);
    if(errors<0){close(io[0]);close(io[1]);return -1;}
    s->input=calloc(1,sizeof(*s->input));s->output=calloc(1,sizeof(*s->output));
    if(!s->input || !s->output || nonblocking(s->client) || nonblocking(io[0])){
        close(errors);close(io[0]);close(io[1]);return -1;
    }
    char *args[]={PS5_AI_STATE "/runtime/codex.elf","app-server","--listen","stdio://",
      "-c","cli_auth_credentials_store=\"file\"","-c","mcp_oauth_credentials_store=\"file\"",NULL};
    /* A new engine must not keep another owner's sockets or control requests open. */
    int inherited[16];size_t count=0;
    inherited[count++]=server;inherited[count++]=admin;
    for(unsigned i=0;i<SESSION_LIMIT;++i){
        if(sessions[i].client>=0)inherited[count++]=sessions[i].client;
        if(sessions[i].io>=0)inherited[count++]=sessions[i].io;
    }
    for(unsigned i=0;i<CONTROL_LIMIT;++i)if(controls[i].fd>=0)inherited[count++]=controls[i].fd;
    inherited[count++]=io[0];inherited[count++]=io[1];inherited[count++]=errors;
    s->child=ps5_spawn_sdk(args[0],args,environ,PS5_AI_STATE "/workspace",
                          io[1],io[1],errors,inherited,count,1);
    close(errors);close(io[1]);
    if(s->child<=0){close(io[0]);return -1;}
    s->io=io[0];return 0;
}
static int send_queue(int fd,struct queue *q,short events){
    if(q->begin==q->end)return 0;
    if(events&(POLLERR|POLLHUP|POLLNVAL))return -1;
    if((events&POLLOUT) && q->end>q->begin){
        ssize_t n=write(fd,q->bytes+q->begin,q->end-q->begin);
        if(n>0){q->begin+=(size_t)n;if(q->begin==q->end)q->begin=q->end=0;}
        else if(n==0 || (errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR))return -1;
    }
    return 0;
}
/* Returns 1 on EOF. Queued output is drained before closing a completed engine. */
static int read_queue(int fd,struct queue *q,short events){
    if(events&POLLIN){
        ssize_t n=read(fd,q->bytes+q->end,sizeof(q->bytes)-q->end);
        if(n>0)q->end+=(size_t)n;
        else if(n==0)return 1;
        else if(errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR)return -1;
    } else if(events&(POLLERR|POLLHUP|POLLNVAL))return 1;
    return 0;
}
static int listener(unsigned port){
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
    int reuse=1;if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))){close(fd);return -1;}
    struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
#if defined(__FreeBSD__)
    addr.sin_len=sizeof(addr);
#endif
    if(bind(fd,(void*)&addr,sizeof(addr)) || listen(fd,4) || nonblocking(fd)){close(fd);return -1;}
    return fd;
}
static unsigned active_sessions(struct session *sessions){
    unsigned count=0;for(unsigned i=0;i<SESSION_LIMIT;++i)if(sessions[i].client>=0)++count;return count;
}
static int control_response(struct control_client *c,unsigned active){
    const int status=!strncmp(c->bytes,"GET /status HTTP/1.1\r\n",22);
    const int stop=!strncmp(c->bytes,"POST /stop HTTP/1.1\r\n",21);
    if(!status && !stop)return 0;
    char body[384],response[640];int length;
    const int busy=stop && active;
    if(busy)length=snprintf(body,sizeof(body),"{\"error\":\"Close all assistant clients before stopping\",\"connections\":%u}\n",active);
    else length=snprintf(body,sizeof(body),"{\"service\":\"codex-ps5\",\"version\":\"%s\",\"build\":\"%s\",\"pid\":%ld,\"idle\":%s,\"connections\":%u}\n",
        CODEX_PS5_VERSION,CODEX_BUILD_ID,(long)getpid(),active?"false":"true",active);
    int size=snprintf(response,sizeof(response),"HTTP/1.1 %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",busy?"409 Conflict":"200 OK",length,body);
    for(int offset=0;offset<size;){
        ssize_t n=write(c->fd,response+offset,(size_t)(size-offset));
        if(n<0 && errno==EINTR)continue;
        if(n<=0)return 0;offset+=(int)n;
    }
    return stop && !busy;
}
int ps5_assistant_run(void){
    int server=listener(ASSISTANT_RPC_PORT);if(server<0)return 1;
    int admin=listener(ASSISTANT_ADMIN_PORT);if(admin<0){close(server);return 1;}
    /* Duplicate launches cannot overwrite an active service's installed runtime. */
    if(ps5_install_runtime_image() || ps5_install_runtime_tools()){close(admin);close(server);return 1;}
    struct session sessions[SESSION_LIMIT];struct control_client controls[CONTROL_LIMIT];
    for(unsigned i=0;i<SESSION_LIMIT;++i)sessions[i]=(struct session){.client=-1,.io=-1};
    for(unsigned i=0;i<CONTROL_LIMIT;++i)controls[i]=(struct control_client){.fd=-1};
    int stopping=0;
    while(!stopping){
        struct pollfd fds[2+CONTROL_LIMIT+SESSION_LIMIT*2];
        fds[0]=(struct pollfd){server,POLLIN,0};fds[1]=(struct pollfd){admin,POLLIN,0};
        for(unsigned i=0;i<CONTROL_LIMIT;++i)fds[2+i]=(struct pollfd){controls[i].fd,POLLIN,0};
        for(unsigned i=0;i<SESSION_LIMIT;++i){
            struct session *s=&sessions[i];short client=0,engine=0;
            if(s->client>=0){
                if(!s->eof && s->input->end<sizeof(s->input->bytes))client|=POLLIN;
                if(s->output->end>s->output->begin)client|=POLLOUT;
                if(!s->eof && s->output->end<sizeof(s->output->bytes))engine|=POLLIN;
                if(!s->eof && s->input->end>s->input->begin)engine|=POLLOUT;
            }
            unsigned index=2+CONTROL_LIMIT+i*2;
            fds[index]=(struct pollfd){s->client,client,0};
            /* Without interest, a hung-up engine would report POLLHUP and spin the loop. */
            fds[index+1]=(struct pollfd){s->eof||!engine?-1:s->io,engine,0};
        }
        if(poll(fds,sizeof(fds)/sizeof(*fds),100)<0){if(errno==EINTR)continue;break;}
        uint64_t now=now_ms();
        for(unsigned i=0;i<SESSION_LIMIT;++i){
            struct session *s=&sessions[i];if(s->client<0)continue;
            unsigned index=2+CONTROL_LIMIT+i*2;short c=fds[index].revents,e=fds[index+1].revents;
            if(send_queue(s->client,s->output,c)){destroy(s);continue;}
            /* A dead engine drops its unsent input but keeps the output it already produced. */
            if(!s->eof && send_queue(s->io,s->input,e))s->input->begin=s->input->end=0;
            if(!s->eof){
                int incoming=read_queue(s->client,s->input,c);
                int reply=s->output->end<sizeof(s->output->bytes)?read_queue(s->io,s->output,e):0;
                if(incoming){destroy(s);continue;}
                if(reply)s->eof=1;
            }
            if(s->eof && s->output->begin==s->output->end)destroy(s);
        }
        for(unsigned i=0;i<CONTROL_LIMIT;++i){
            struct control_client *c=&controls[i];if(c->fd<0)continue;
            short events=fds[2+i].revents;int done=now>=c->deadline;
            if(events&POLLIN){
                ssize_t n=read(c->fd,c->bytes+c->used,sizeof(c->bytes)-1-c->used);
                if(n>0){c->used+=(size_t)n;c->bytes[c->used]=0;
                    if(strstr(c->bytes,"\r\n\r\n")){stopping=control_response(c,active_sessions(sessions));done=1;}
                    if(c->used==sizeof(c->bytes)-1)done=1;
                } else if(n==0 || (errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR))done=1;
            }
            if(events&(POLLERR|POLLHUP|POLLNVAL))done=1;
            if(done){close(c->fd);c->fd=-1;}
            if(stopping)break;
        }
        if(stopping)break;
        if(fds[1].revents&POLLIN){
            int fd=accept(admin,NULL,NULL);if(fd>=0){
                unsigned slot;for(slot=0;slot<CONTROL_LIMIT && controls[slot].fd>=0;++slot){}
                if(slot==CONTROL_LIMIT || nonblocking(fd))close(fd);
                else controls[slot]=(struct control_client){.fd=fd,.deadline=now+2000};
            }
        }
        if(fds[0].revents&POLLIN){
            int fd=accept(server,NULL,NULL);if(fd>=0){
                unsigned slot;for(slot=0;slot<SESSION_LIMIT && sessions[slot].client>=0;++slot){}
                if(slot==SESSION_LIMIT)close(fd);
                else {sessions[slot].client=fd;if(start(&sessions[slot],server,admin,sessions,controls))destroy(&sessions[slot]);}
            }
        }
    }
    for(unsigned i=0;i<SESSION_LIMIT;++i)destroy(&sessions[i]);
    for(unsigned i=0;i<CONTROL_LIMIT;++i)if(controls[i].fd>=0)close(controls[i].fd);
    close(admin);close(server);return 0;
}
