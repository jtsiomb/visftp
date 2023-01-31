#include "viewer.h"
#include "tgfx.h"
#include "darray.h"
#include "util.h"
#include "input.h"

struct span {
	char *start;
	long len;
};

static char *title;
static struct span *lines;
static void *data;
static long datasz;
static int dirty;
static int hscroll, vscroll;
static int num_vislines = 22;
static int num_viscol = 78;

static void (*freedata)(void*);


void view_memopen(const char *name, void *start, long size, void (*freefunc)(void*))
{
	char *ptr;
	struct span span;

	data = start;
	datasz = size;

	title = strdup_nf(name);

	lines = darr_alloc(0, sizeof *lines);
	ptr = start;
	while(size--) {
		if(*ptr == '\r' || *ptr == '\n') {
			if(ptr > (char*)start) {
				span.start = start;
				span.len = ptr - (char*)start;
				darr_push(lines, &span);
			}
			start = ptr + 1;
		}
		ptr++;
	}

	freedata = freefunc;
	dirty = 1;
}

void view_close(void)
{
	darr_free(lines);
	if(freedata) {
		freedata(data);
	} else {
		free(data);
	}
	data = 0;
}

int view_isopen(void)
{
	return data != 0;
}

void view_update(void)
{
	int x, i, j, c;
	char *ptr;
	struct span *span;

	if(!dirty) return;

	tg_fgcolor(TGFX_WHITE);
	tg_bgcolor(TGFX_BLUE);

	tg_rect(title, 0, 0, 80, 23, TGFX_FRAME);

	span = lines + vscroll;
	for(i=0; i<darr_size(lines) && i < num_vislines - 1; i++) {
		x = 0;
		ptr = span->start;

		for(j=0; j<span->len; j++) {
			if(x >= num_viscol) break;

			c = *ptr++;

			if(c == '\t') {
				x = (x + 4) & 0xfffc;
			} else {
				if(++x - hscroll > 0) {
					tg_putchar(x - hscroll, i + 1, c);
				}
			}
		}
		span++;
	}

	dirty = 0;
	tg_redraw();
}

void view_input(int key)
{
	switch(key) {
	case 27:
	case KB_F10:
		view_close();
		break;

	case KB_HOME:
		vscroll = hscroll = 0;
		break;
	case KB_END:
		vscroll = darr_size(lines) - num_vislines + 1;
		if(vscroll < 0) vscroll = 0;
		hscroll = 0;
		break;

	case KB_PGUP:
		vscroll -= num_vislines;
		if(vscroll < 0) vscroll = 0;
		hscroll = 0;
		break;
	case KB_PGDN:
		vscroll += num_vislines;
		if(vscroll >= darr_size(lines)) {
			view_input(KB_END);
		}
		hscroll = 0;
		break;

	case KB_UP:
		if(vscroll > 0) vscroll--;
		break;
	case KB_DOWN:
		if(vscroll < darr_size(lines) - 1) {
			vscroll++;
		}
		break;
	case KB_LEFT:
		if(hscroll > 0) hscroll--;
		break;
	case KB_RIGHT:
		hscroll++;
		break;

	default:
		return;
	}

	dirty = 1;
}
