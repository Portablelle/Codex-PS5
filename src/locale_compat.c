/* SPDX-License-Identifier: GPL-3.0-or-later
 * C-locale adapter for the FreeBSD libc++ static archive. Native Sony libc
 * lacks the *_l entry points. No global locale mutation or locale ownership.
 * UTF-8 UI decoding is handled independently by renderer.cpp.
 */
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <limits.h>
#include <runetype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <time.h>
#include <wchar.h>
#include <xlocale.h>
long long strtoll_l(const char* s,char** end,int base,locale_t l){(void)l;return strtoll(s,end,base);}
unsigned long long strtoull_l(const char* s,char** end,int base,locale_t l){(void)l;return strtoull(s,end,base);}
float strtof_l(const char* s,char** end,locale_t l){(void)l;return strtof(s,end);}
double strtod_l(const char* s,char** end,locale_t l){(void)l;return strtod(s,end);}
long double strtold_l(const char* s,char** end,locale_t l){(void)l;return strtold(s,end);}
int sscanf_l(const char* s,locale_t l,const char* fmt,...){(void)l;va_list a;va_start(a,fmt);int n=vsscanf(s,fmt,a);va_end(a);return n;}
int snprintf_l(char* out,size_t len,locale_t l,const char* fmt,...){(void)l;va_list a;va_start(a,fmt);int n=vsnprintf(out,len,fmt,a);va_end(a);return n;}
int asprintf_l(char** out,locale_t l,const char* fmt,...){(void)l;va_list a;va_start(a,fmt);int n=vasprintf(out,fmt,a);va_end(a);return n;}
size_t strftime_l(char* out,size_t n,const char* fmt,const struct tm* t,locale_t l){(void)l;return strftime(out,n,fmt,t);}
int freelocale(locale_t l){(void)l;return 0;} // libc++ only uses the process C locale here
size_t mbsrtowcs_l(wchar_t* out,const char** in,size_t n,mbstate_t* st,locale_t l){(void)l;return mbsrtowcs(out,in,n,st);}
size_t wcrtomb_l(char* out,wchar_t ch,mbstate_t* st,locale_t l){(void)l;return wcrtomb(out,ch,st);}
size_t mbrtowc_l(wchar_t* out,const char* in,size_t n,mbstate_t* st,locale_t l){(void)l;return mbrtowc(out,in,n,st);}
int mbtowc_l(wchar_t* out,const char* in,size_t n,locale_t l){(void)l;return mbtowc(out,in,n);}
size_t mbrlen_l(const char* in,size_t n,mbstate_t* st,locale_t l){(void)l;return mbrlen(in,n,st);}
int ___mb_cur_max_l(locale_t l){(void)l;return 1;}
size_t mbsnrtowcs_l(wchar_t* out,const char** in,size_t bytes,size_t length,mbstate_t* st,locale_t l){
 (void)l;const char* p=*in;size_t count=0;
 while(bytes && (!out || count<length)){
  wchar_t ch;size_t n=mbrtowc(&ch,p,bytes,st);
  if(n==(size_t)-1)return n;
  if(n==(size_t)-2){if(out)*in=p+bytes;return count;}
  if(!n){if(out){out[count]=0;*in=NULL;}return count;}
  if(out)out[count]=ch;++count;p+=n;bytes-=n;
 }
 if(out)*in=p;return count;
}
size_t wcsnrtombs_l(char* out,const wchar_t** in,size_t chars,size_t length,mbstate_t* st,locale_t l){
 (void)l;const wchar_t* p=*in;size_t total=0;
 while(chars){char block[MB_LEN_MAX];mbstate_t saved=*st;size_t n=wcrtomb(block,*p,st);
  if(n==(size_t)-1)return n;
  if(out && n>length-total){*st=saved;break;}
  if(out)for(size_t i=0;i<n;++i)out[total+i]=block[i];
  if(!*p){if(out)*in=NULL;return total+n-1;}
  total+=n;++p;--chars;
 }
 if(out)*in=p;return total;
}
/* No translated message catalogs; callers retain their fallback text. */
void* catopen(const char* path,int mode){(void)path;(void)mode;errno=ENOENT;return (void*)-1;}
char* catgets(void* catalog,int set,int message,const char* fallback){(void)catalog;(void)set;(void)message;return (char*)fallback;}
int catclose(void* catalog){(void)catalog;return 0;}
_RuneLocale* __runes_for_locale(locale_t l,int* limit){
 (void)l;static _RuneLocale runes;static atomic_int ready;
 if(atomic_load(&ready)!=2){int expected=0;
  if(atomic_compare_exchange_strong(&ready,&expected,1)){
   for(int c=0;c<256;++c){unsigned long m=0;
    if(c<128){if(c>='A'&&c<='Z')m|=_CTYPE_A|_CTYPE_U;if(c>='a'&&c<='z')m|=_CTYPE_A|_CTYPE_L;
     if(c>='0'&&c<='9')m|=_CTYPE_D;if((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'))m|=_CTYPE_X;
     if(c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f')m|=_CTYPE_S;
     if(c==' '||c=='\t')m|=_CTYPE_B;if(c<32||c==127)m|=_CTYPE_C;
     if(c>=32&&c<127)m|=_CTYPE_R;if(c>32&&c<127)m|=_CTYPE_G;
     if(c>32&&c<127 && !(m&(_CTYPE_A|_CTYPE_D)))m|=_CTYPE_P;
    }
    runes.__runetype[c]=m;runes.__maplower[c]=(c>='A'&&c<='Z')?c+32:c;runes.__mapupper[c]=(c>='a'&&c<='z')?c-32:c;
   }
   atomic_store(&ready,2);
  }else while(atomic_load(&ready)!=2){}
 }
 *limit=256;return &runes;
}
