/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <sys/types.h>
#include <sys/mount.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static int human;
static void amount(uint64_t value,char out[32]) {
 if(!human){snprintf(out,32,"%" PRIu64,value/1024);return;}
 const char* units[]={"B","KiB","MiB","GiB","TiB","PiB"};unsigned u=0;double n=(double)value;
 while(n>=1024 && u<5){n/=1024;++u;}snprintf(out,32,"%.1f%s",n,units[u]);
}
static int row(const struct statfs* s) {
 if(!s->f_bsize || s->f_blocks>UINT64_MAX/s->f_bsize || s->f_bfree>s->f_blocks ||
    (s->f_bavail>0 && (uint64_t)s->f_bavail>UINT64_MAX/s->f_bsize)){
  fprintf(stderr,"df: invalid filesystem counters\n");return 1;
 }
 uint64_t available=s->f_bavail>0?(uint64_t)s->f_bavail:0;
 char size[32],used[32],free_space[32];
 amount(s->f_blocks*s->f_bsize,size);amount((s->f_blocks-s->f_bfree)*s->f_bsize,used);amount(available*s->f_bsize,free_space);
 const double occupied=(double)(s->f_blocks-s->f_bfree);
 const double denominator=occupied+(double)available;
 printf("%s\t%s\t%s\t%s\t%.0f%%\t%s\t%s\n",s->f_mntfromname,size,used,free_space,denominator?100*occupied/denominator:0,s->f_fstypename,s->f_mntonname);return 0;
}
static int relevant(const struct statfs* s) {
 return (!strcmp(s->f_mntonname,"/user") || !strcmp(s->f_mntonname,"/data") ||
   !strncmp(s->f_mntonname,"/mnt/usb",8) || !strncmp(s->f_mntonname,"/mnt/ext",8)) &&
   strcmp(s->f_fstypename,"nullfs") && strcmp(s->f_fstypename,"devfs") && strcmp(s->f_fstypename,"tmpfs");
}
int main(int argc,char**argv) {
 int all=0,first=1,result=0;
 for(;first<argc && argv[first][0]=='-';++first){
  if(!strcmp(argv[first],"--")){++first;break;}
  if(!strcmp(argv[first],"-h"))human=1;
  else if(!strcmp(argv[first],"-a"))all=1;
  else if(!strcmp(argv[first],"-k") || !strcmp(argv[first],"-P")){}
  else {fprintf(stderr,"Usage: df [-h] [-a] [-k] [-P] [--] [path ...]\n");return 2;}
 }
 printf("Filesystem\t%s\tUsed\tAvailable\tUse%%\tType\tMounted on\n",human?"Size":"1K-blocks");
 if(first<argc){
  for(;first<argc;++first){struct statfs s;
   if(statfs(argv[first],&s)){fprintf(stderr,"df: %s: %s\n",argv[first],strerror(errno));result=1;}
   else result|=row(&s);
  }
 }else{
  struct statfs* mounts=NULL;int count=getmntinfo(&mounts,MNT_NOWAIT),shown=0;
  if(count<=0){fprintf(stderr,"df: mounted-volume enumeration failed: %s\n",strerror(errno));return 1;}
  for(int i=0;i<count;++i)if(all || relevant(&mounts[i])){result|=row(&mounts[i]);++shown;}
  if(!shown){fprintf(stderr,"df: no user-storage mount was discovered; use df -a or an explicit path\n");return 1;}
 }
 return result;
}
