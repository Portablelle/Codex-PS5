/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char**argv){
 int first=1,names=1,raw=0,result=0;
 while(first<argc && argv[first][0]=='-'){
  if(!strcmp(argv[first],"-n"))names=0;
  else if(!strcmp(argv[first],"-b")){raw=1;names=0;}
  else {fprintf(stderr,"Usage: sysctl [-n|-b] NAME ... (read-only; -b exposes opaque values)\n");return 2;}++first;
 }
 if(first==argc){fprintf(stderr,"sysctl: a named kernel property is required\n");return 2;}
 for(;first<argc;++first){
  int mib[CTL_MAXNAME],fmtmib[CTL_MAXNAME+2];size_t count=CTL_MAXNAME,length=0;
  if(strchr(argv[first],'=') || sysctlnametomib(argv[first],mib,&count) || sysctl(mib,count,NULL,&length,NULL,0) || length>4*1024*1024){fprintf(stderr,"sysctl: cannot read %s: %s\n",argv[first],strerror(errno));result=1;continue;}
  void*value=calloc(1,length+1);if(!value || sysctl(mib,count,value,&length,NULL,0)){fprintf(stderr,"sysctl: %s: %s\n",argv[first],strerror(errno));free(value);result=1;continue;}
  if(raw){fwrite(value,1,length,stdout);free(value);continue;}
  /* FreeBSD/Prospero sysctl metadata: sysctl.oidfmt = {0,4}. */
  fmtmib[0]=0;fmtmib[1]=4;memcpy(fmtmib+2,mib,count*sizeof(int));
  unsigned char format[256]={0};size_t flen=sizeof(format);unsigned kind=0;
  if(!sysctl(fmtmib,count+2,format,&flen,NULL,0) && flen>=sizeof(kind))memcpy(&kind,format,sizeof(kind));
  kind&=CTLTYPE;int known=1;
  if(names)printf("%s: ",argv[first]);
  if(kind==CTLTYPE_STRING)printf("%.*s",(int)length,(char*)value);
  else if(kind==CTLTYPE_INT && length==4){int32_t v;memcpy(&v,value,4);printf("%"PRId32,v);}
  else if(kind==CTLTYPE_UINT && length==4){uint32_t v;memcpy(&v,value,4);printf("%"PRIu32,v);}
  else if((kind==CTLTYPE_LONG || kind==CTLTYPE_S64) && length==8){int64_t v;memcpy(&v,value,8);printf("%"PRId64,v);}
  else if((kind==CTLTYPE_ULONG || kind==CTLTYPE_U64) && length==8){uint64_t v;memcpy(&v,value,8);printf("%"PRIu64,v);}
  else {known=0;printf("opaque (%zu bytes; use -b)",length);}
  putchar('\n');free(value);if(!known)result=1;
 }
 return result;
}
