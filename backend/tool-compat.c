/* SPDX-License-Identifier: GPL-3.0-or-later
 * Missing libc entry points used by the portable command set. */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <stddef.h>
#include <time.h>
int sethostname(const char *name,size_t length){
 return sysctlbyname("kern.hostname",NULL,NULL,name,length);
}
/* Numeric calendar formats used by sbase touch; reject unsupported directives. */
char*strptime(const char*value,const char*format,struct tm*tm){
 const char*p=value;
 for(;*format;++format){
  if(*format!='%'){if(*p++!=*format)return NULL;continue;}
  ++format;int width=2,min=0,max=0,*target=NULL,adjust=0;
  switch(*format){
   case 'Y':width=4;max=9999;target=&tm->tm_year;adjust=-1900;break;
   case 'y':max=99;target=&tm->tm_year;break;
   case 'm':min=1;max=12;target=&tm->tm_mon;adjust=-1;break;
   case 'd':min=1;max=31;target=&tm->tm_mday;break;
   case 'H':max=23;target=&tm->tm_hour;break;
   case 'M':max=59;target=&tm->tm_min;break;
   case 'S':max=60;target=&tm->tm_sec;break;
   default:return NULL;
  }
  int n=0;for(int i=0;i<width;++i){if(*p<'0'||*p>'9')return NULL;n=n*10+*p++-'0';}
  if(n<min || n>max)return NULL;
  *target=n+adjust;if(*format=='y')tm->tm_year=n+(n<=68?100:0);
 }
 return (char*)p;
}
