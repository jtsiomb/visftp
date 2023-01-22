#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "darray.h"
#include "tui.h"
#include "tuipriv.h"
#include "tgfx.h"
#include "util.h"

static void free_list(struct tui_widget *w, void *cls);
static void draw_list(struct tui_widget *w, void *cls);

struct tui_widget *tui_list(const char *title, int x, int y, int width, int height, tui_callback cbfunc, void *cbdata)
{
	struct tui_list *w;

	if(!(w = calloc(1, sizeof *w))) {
		return 0;
	}
	w->type = TUI_LIST;
	w->title = strdup(title);
	w->x = x;
	w->y = y;
	w->width = width;
	w->height = height;

	w->entries = darr_alloc(0, sizeof *w->entries);
	w->sel = -1;
	w->dirty = 1;

	if(cbfunc) {
		tui_set_callback((struct tui_widget*)w, TUI_ONMODIFY, cbfunc, cbdata);
	}
	tui_set_callback((struct tui_widget*)w, TUI_FREE, free_list, 0);
	tui_set_callback((struct tui_widget*)w, TUI_DRAW, draw_list, 0);
	return (struct tui_widget*)w;
}

static void free_list(struct tui_widget *w, void *cls)
{
	struct tui_list *wl = (struct tui_list*)w;

	tui_clear_list(w);
	darr_free(wl->entries);
}


void tui_clear_list(struct tui_widget *w)
{
	int i;
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	for(i=0; i<darr_size(wl->entries); i++) {
		free(wl->entries[i]);
	}
	darr_clear(wl->entries);
	wl->dirty = 1;
}

void tui_add_list_item(struct tui_widget *w, const char *text)
{
	char *str;
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);
	str = strdup_nf(text);
	darr_push(wl->entries, &str);
	wl->dirty = 1;
}

int tui_num_list_items(struct tui_widget *w)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);
	return darr_size(wl->entries);
}

int tui_list_select(struct tui_widget *w, int idx)
{
	int offs, nelem;
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	if(idx == wl->sel) {
		return 0;	/* no change */
	}

	if(idx < 0) {
		wl->sel = -1;
		return 0;
	}
	if(idx >= (nelem = darr_size(wl->entries))) {
		return -1;
	}
	wl->sel = idx;

	if(idx < wl->view_offs || idx >= wl->view_offs + wl->height) {
		offs = idx - wl->height / 2;
		if(offs + wl->height >= nelem) {
			offs = nelem - wl->height;
		}
		if(offs < 0) {
			offs = 0;
		}
		wl->view_offs = offs;
	}

	wl->dirty = 1;
	return 0;
}

int tui_get_list_sel(struct tui_widget *w)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);
	return wl->sel;
}

int tui_list_sel_next(struct tui_widget *w)
{
	int nelem;
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	nelem = darr_size(wl->entries);

	if(wl->sel + 1 >= nelem) {
		return -1;
	}

	if(++wl->sel - wl->view_offs >= wl->height) {
		wl->view_offs = wl->sel - wl->height;
	}
	wl->dirty = 1;
	return 0;
}

int tui_list_sel_prev(struct tui_widget *w)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	if(wl->sel <= 0) {
		return -1;
	}
	if(--wl->sel < wl->view_offs) {
		wl->view_offs = wl->sel;
	}
	wl->dirty = 1;
	return 0;
}

int tui_list_sel_start(struct tui_widget *w)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	wl->sel = 0;
	wl->view_offs = 0;
	wl->dirty = 1;
	return 0;
}

int tui_list_sel_end(struct tui_widget *w)
{
	int nelem;
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	nelem = darr_size(wl->entries);

	wl->sel = nelem - 1;
	wl->view_offs = nelem - wl->height;
	if(wl->view_offs < 0) wl->view_offs = 0;
	wl->dirty = 1;
	return 0;
}

void tui_sort_list(struct tui_widget *w, int (*cmpfunc)(const void*, const void*))
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	if(!cmpfunc) {
		cmpfunc = (int (*)(const void*, const void*))strcmp;
	}
	qsort(wl->entries, darr_size(wl->entries), sizeof *wl->entries, cmpfunc);
}

static void draw_list(struct tui_widget *w, void *cls)
{
	int i, x, y, num, idx;
	struct tui_list *wl = (struct tui_list*)w;

	tui_wtoscr(w, 0, 0, &x, &y);

	tg_bgcolor(TGFX_BLUE);
	tg_fgcolor(TGFX_CYAN);
	tg_rect(wl->title, x, y, wl->width, wl->height, TGFX_FRAME);

	num = darr_size(wl->entries);
	if(num > wl->height - 2) {
		num = wl->height - 2;
	}

	x++;
	for(i=0; i<num; i++) {
		idx = i + wl->view_offs;
		if(idx == wl->sel) {
			tg_bgcolor(TGFX_CYAN);
			tg_fgcolor(TGFX_BLUE);

			tg_rect(0, x, ++y, wl->width-2, 1, 0);
			tg_text(x, y, "%s", wl->entries[idx]);

			tg_bgcolor(TGFX_BLUE);
			tg_fgcolor(TGFX_CYAN);
		} else {
			tg_text(x, ++y, "%s", wl->entries[idx]);
		}
	}
}
