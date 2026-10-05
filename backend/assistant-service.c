/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "service-version.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "../platform/sdk-spawn.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
extern int ps5_install_runtime_image(void);
extern int ps5_install_runtime_tools(void);
/* Bounded duplex buffers. No credentials or conversation content are logged. */
struct queue { unsigned char bytes[65536]; size_t begin, end; };
static int pump(int input,int output,struct queue *q,short in,short out) {
    if(out & (POLLERR|POLLHUP|POLLNVAL)) return -1;
    if((out&POLLOUT) && q->end>q->begin) {
        ssize_t n=write(output,q->bytes+q->begin,q->end-q->begin);
        if(n>0) {q->begin+=(size_t)n;if(q->begin==q->end)q->begin=q->end=0;}
        else if(n==0 || (errno!=EAGAIN && errno!=EINTR))return -1;
    }
    if(in&POLLIN) {
        ssize_t n=read(input,q->bytes+q->end,sizeof(q->bytes)-q->end);
        if(n>0)q->end+=(size_t)n;
        else if(n==0 || (errno!=EAGAIN && errno!=EINTR))return -1;
    } else if(in & (POLLERR|POLLHUP|POLLNVAL)) return -1;
    return 0;
}
static void session(int client,int listener,int admin) {
    int io[2];
    if(socketpair(AF_UNIX,SOCK_STREAM,0,io))return;
    int errors=open("/dev/null",O_WRONLY);
    if(errors<0){close(io[0]);close(io[1]);return;}
    char *args[]={PS5_AI_STATE "/runtime/codex.elf","app-server","--listen","stdio://",
      "-c","cli_auth_credentials_store=\"file\"","-c","mcp_oauth_credentials_store=\"file\"",NULL};
    const int inherited[]={client,listener,admin,io[0],io[1],errors};
    pid_t child=ps5_spawn_sdk(args[0],args,environ,PS5_AI_STATE "/workspace",
                              io[1],io[1],errors,inherited,6,1);
    close(errors);close(io[1]);
    if(child<=0){close(io[0]);return;}
    fcntl(client,F_SETFL,fcntl(client,F_GETFL)|O_NONBLOCK);
    fcntl(io[0],F_SETFL,fcntl(io[0],F_GETFL)|O_NONBLOCK);
    struct queue *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));
    if(a && b)for(;;) {
        struct pollfd fds[2]={{client,0,0},{io[0],0,0}};
        if(a->end<sizeof(a->bytes))fds[0].events|=POLLIN;
        if(b->end>b->begin)fds[0].events|=POLLOUT;
        if(b->end<sizeof(b->bytes))fds[1].events|=POLLIN;
        if(a->end>a->begin)fds[1].events|=POLLOUT;
        if(poll(fds,2,500)<0){if(errno==EINTR)continue;break;}
        if(pump(client,io[0],a,fds[0].revents,fds[1].revents) ||
           pump(io[0],client,b,fds[1].revents,fds[0].revents))break;
    }
    free(a);free(b);close(io[0]);
    /* process_mode=1 creates a process group; terminate only our owned process group. */
    kill(-child,SIGTERM);
    int status,ended=0;
    for(unsigned attempts=0;attempts<20;++attempts){
        pid_t result=waitpid(child,&status,WNOHANG);
        if(result==child || (result<0 && errno!=EINTR)){ended=1;break;}
        usleep(50000);
    }
    if(!ended){
        pid_t result;
        do {result=waitpid(child,&status,WNOHANG);}while(result<0 && errno==EINTR);
        /* A PID stays owned until it is reaped. Never signal a reaped PID. */
        if(result==0){kill(-child,SIGKILL);while(waitpid(child,&status,0)<0 && errno==EINTR){}}
    }
}
/* Separate loopback control listener. The session loop never services a stop
 * while a conversation is connected; callers must close the native app first. */
static int listener(unsigned port) {
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
    int reuse=1;if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))){close(fd);return -1;}
    struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
#if defined(__FreeBSD__)
    addr.sin_len=sizeof(addr);
#endif
    if(bind(fd,(void*)&addr,sizeof(addr)) || listen(fd,4)){close(fd);return -1;}
    return fd;
}
static int control(int fd){
    struct timeval timeout={2,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    char request[1024]={0};size_t used=0;
    while(used<sizeof(request)-1){ssize_t n=read(fd,request+used,sizeof(request)-1-used);if(n<=0)return 0;used+=(size_t)n;request[used]=0;if(strstr(request,"\r\n\r\n"))break;}
    const int status=!strncmp(request,"GET /status HTTP/1.1\r\n",22);
    const int stop=!strncmp(request,"POST /stop HTTP/1.1\r\n",21);
    if(!status && !stop)return 0;
    char body[256],response[512];
    int length=snprintf(body,sizeof(body),"{\"service\":\"codex-ps5\",\"version\":\"%s\",\"build\":\"%s\",\"pid\":%ld,\"idle\":true}\n",CODEX_PS5_VERSION,CODEX_BUILD_ID,(long)getpid());
    int size=snprintf(response,sizeof(response),"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",length,body);
    for(int offset=0;offset<size;){ssize_t n=write(fd,response+offset,(size_t)(size-offset));if(n<=0)return 0;offset+=(int)n;}
    return stop;
}
int ps5_assistant_run(void) {
    int server=listener(49322);if(server<0)return 1;
    int admin=listener(49323);if(admin<0){close(server);return 1;}
    /* A duplicate launch cannot overwrite the runtime of the active process. */
    if(ps5_install_runtime_image() || ps5_install_runtime_tools()){close(admin);close(server);return 1;}
    for(;;){
        struct pollfd ports[2]={{admin,POLLIN,0},{server,POLLIN,0}};
        if(poll(ports,2,-1)<0){if(errno==EINTR)continue;break;}
        if(ports[0].revents&POLLIN){int fd=accept(admin,NULL,NULL);if(fd>=0){int stop=control(fd);close(fd);if(stop)break;}}
        if(ports[1].revents&POLLIN){int fd=accept(server,NULL,NULL);if(fd>=0){session(fd,server,admin);close(fd);}}
    }
    close(admin);close(server);return 0;
}
