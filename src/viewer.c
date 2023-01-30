#include "tgfx.h"
#include "darray.h"
#include "util.h"

struct span {
	void *start;
	long len;
};

static char *title;
static struct span *lines;
static void *data;
static long datasz;

void view_memopen(const char *name, void *start, long size)
{
	data = start;
	datasz = size;

	title = strdup_nf(name);
}

void view_close(void)
{
	darr_free(lines);
	free(data);
	data = 0;
}

int view_isopen(void)
{
	return data != 0;
}

void view_draw(void)
{
}

void view_input(int key)
{
}
