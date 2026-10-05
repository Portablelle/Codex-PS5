/* Link-time import provider only. Never deploy this library. */
#include <stdint.h>
int32_t sceAudioInOpen(int32_t u,int32_t t,int32_t i,uint32_t g,uint32_t f,uint32_t fmt){(void)u;(void)t;(void)i;(void)g;(void)f;(void)fmt;return -1;}
int32_t sceAudioInInput(int32_t h,void* b){(void)h;(void)b;return -1;}
int32_t sceAudioInClose(int32_t h){(void)h;return -1;}
int32_t sceAudioInGetSilentState(int32_t h){(void)h;return -1;}
