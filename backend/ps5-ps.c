/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Prospero process records: the stable prefix used by FW 13.00 inspection.
 * Do not treat the SDK's FreeBSD ki_comm length as the Prospero name length. */
static int usage(void){fprintf(stderr,"Usage: ps [-A|-e|-a|-x|-ax|aux|-ef] [-p PID]\nOutputs PID and process name, not arguments, CPU or thread state.\n");return 2;}
int main(int argc,char **argv){
 long wanted=-1;
 for(int i=1;i<argc;++i){
  if(!strcmp(argv[i],"--help"))return usage();
  if(!strcmp(argv[i],"-p") && i+1<argc){char*end;errno=0;wanted=strtol(argv[++i],&end,10);if(errno||*end||wanted<0)return usage();}
  else if(strcmp(argv[i],"-A") && strcmp(argv[i],"-e") && strcmp(argv[i],"-a") && strcmp(argv[i],"-x") && strcmp(argv[i],"-ax") && strcmp(argv[i],"aux") && strcmp(argv[i],"-ef"))return usage();
 }
 int mib[]={CTL_KERN,KERN_PROC,KERN_PROC_PROC,0};size_t length=0;unsigned char*data=NULL;
 for(int attempt=0;attempt<3;++attempt){
  if(sysctl(mib,4,NULL,&length,NULL,0))goto failure;
  if(length>4*1024*1024){errno=EOVERFLOW;goto failure;}length+=65536;
  data=malloc(length);if(!data)goto failure;
  if(!sysctl(mib,4,data,&length,NULL,0))break;
  free(data);data=NULL;if(errno!=ENOMEM)goto failure;
 }
 if(!data)goto failure;
 puts("PID\tCOMMAND");size_t offset=0;
 while(offset<length){
  int32_t size,pid;
  if(length-offset<480){errno=EPROTO;goto failure;}
  memcpy(&size,data+offset,4);memcpy(&pid,data+offset+72,4);
  if(size<480 || (size_t)size>length-offset || pid<0){errno=EPROTO;goto failure;}
  char name[33]={0};memcpy(name,data+offset+447,32);
  for(size_t i=0;i<32 && name[i];++i)if((unsigned char)name[i]<32 || name[i]==127)name[i]='?';
  if(wanted<0 || wanted==pid)printf("%d\t%s\n",pid,name);
  offset+=(size_t)size;
 }
 free(data);return 0;
failure:fprintf(stderr,"ps: cannot enumerate processes: %s\n",strerror(errno));free(data);return 1;
}
