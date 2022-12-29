#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

void *malloc_nf_impl(size_t sz, const char *file, int line)
{
	void *p;
	if(!(p = malloc(sz))) {
		fprintf(stderr, "%s:%d failed to allocate %lu bytes\n", file, line, (unsigned long)sz);
		abort();
	}
	return p;
}

void *calloc_nf_impl(size_t num, size_t sz, const char *file, int line)
{
	void *p;
	if(!(p = calloc(num, sz))) {
		fprintf(stderr, "%s:%d failed to allocate %lu bytes\n", file, line, (unsigned long)(sz * num));
		abort();
	}
	return p;
}

void *realloc_nf_impl(void *p, size_t sz, const char *file, int line)
{
	if(!(p = realloc(p, sz))) {
		fprintf(stderr, "%s:%d failed to realloc %lu bytes\n", file, line, (unsigned long)sz);
		abort();
	}
	return p;
}

char *strdup_nf_impl(const char *s, const char *file, int line)
{
	char *res;
	if(!(res = strdup(s))) {
		fprintf(stderr, "%s:%d failed to duplicate string\n", file, line);
		abort();
	}
	return res;
}


int match_prefix(const char *str, const char *prefix)
{
	while(*str && *prefix) {
		if(*str++ != *prefix++) {
			return 0;
		}
	}
	return *prefix ? 0 : 1;
}
