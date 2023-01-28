#ifndef UTIL_H_
#define UTIL_H_

#include <stdio.h>
#include <stdlib.h>

#if defined(__WATCOMC__) || defined(WIN32)
#include <malloc.h>
#else
#if !defined(__FreeBSD__) && !defined(__OpenBSD__)
#include <alloca.h>
#endif
#endif

#ifdef _MSC_VER
#define strcasecmp(s, k) stricmp(s, k)
#endif

#ifdef __WATCOMC__
#if __WATCOMC__ < 1200
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif
#endif

#define malloc_nf(sz)	malloc_nf_impl(sz, __FILE__, __LINE__)
void *malloc_nf_impl(size_t sz, const char *file, int line);

#define calloc_nf(num, sz)	calloc_nf_impl(num, sz, __FILE__, __LINE__)
void *calloc_nf_impl(size_t num, size_t sz, const char *file, int line);

#define realloc_nf(p, sz)	realloc_nf_impl(p, sz, __FILE__, __LINE__)
void *realloc_nf_impl(void *p, size_t sz, const char *file, int line);

#define strdup_nf(s)	strdup_nf_impl(s, __FILE__, __LINE__)
char *strdup_nf_impl(const char *s, const char *file, int line);

int match_prefix(const char *str, const char *prefix);

void errmsg(const char *fmt, ...);
void warnmsg(const char *fmt, ...);
void infomsg(const char *fmt, ...);

#endif	/* UTIL_H_ */
