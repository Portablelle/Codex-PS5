/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
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
static void session(int client,int listener) {
    int io[2];
    if(socketpair(AF_UNIX,SOCK_STREAM,0,io))return;
    int errors=open("/dev/null",O_WRONLY);
    if(errors<0){close(io[0]);close(io[1]);return;}
    char *args[]={PS5_AI_STATE "/runtime/codex.elf","app-server","--listen","stdio://",
      "-c","cli_auth_credentials_store=\"file\"","-c","mcp_oauth_credentials_store=\"file\"",NULL};
    const int inherited[]={client,listener,io[0],io[1],errors};
    pid_t child=ps5_spawn_sdk(args[0],args,environ,PS5_AI_STATE "/workspace",
                              io[1],io[1],errors,inherited,5,1);
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
int ps5_assistant_run(void) {
    int server=socket(AF_INET,SOCK_STREAM,0);if(server<0)return 1;
    struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_port=htons(49322);
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
#if defined(__FreeBSD__)
    addr.sin_len=sizeof(addr);
#endif
    if(bind(server,(void*)&addr,sizeof(addr)) || listen(server,1)) {close(server);return 1;}
    /* A duplicate launch cannot overwrite the runtime of the active process. */
    if(ps5_install_runtime_image() || ps5_install_runtime_tools()){close(server);return 1;}
    for(;;) {
        int fd=accept(server,NULL,NULL);
        if(fd<0){if(errno==EINTR)continue;break;}
        session(fd,server);close(fd);
    }
    close(server);return 1;
}
