/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
/* Dedicated native heap: model, inference tensors, UI and networking. */
const size_t ps5_opengl_heap_size=1024u*1024u*1024u;
void __assert(const char* expression,const char* file,int line){(void)expression;(void)file;(void)line;abort();}
/* ggml's optional crash debugger has no equivalent inside a native title. */
int execlp(const char* file,const char* arg,...){(void)file;(void)arg;errno=ENOSYS;return -1;}
