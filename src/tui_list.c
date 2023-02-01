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
	w->cur = -1;
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
		free(wl->entries[i].text);
	}
	darr_clear(wl->entries);
	wl->dirty = 1;

	tui_call_callback(w, TUI_ONMODIFY);
}

void tui_add_list_item(struct tui_widget *w, const char *text)
{
	struct tui_list *wl = (struct tui_list*)w;
	struct list_entry item;

	assert(wl->type == TUI_LIST);
	item.text = strdup_nf(text);
	item.sel = 0;
	darr_push(wl->entries, &item);
	wl->dirty = 1;

	tui_call_callback(w, TUI_ONMODIFY);
}

int tui_num_list_items(struct tui_widget *w)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);
	return darr_size(wl->entries);
}

#define VISLINES(wl)	((wl)->height - 2)

int tui_list_setcur(struct tui_widget *w, int idx)
{
	int offs, nelem, numvis;
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	if(idx == wl->cur) {
		return 0;	/* no change */
	}

	numvis = VISLINES(wl);

	if(idx < 0) {
		wl->cur = -1;
		return 0;
	}
	if(idx >= (nelem = darr_size(wl->entries))) {
		return -1;
	}
	wl->cur = idx;

	if(idx < wl->view_offs || idx >= wl->view_offs + numvis) {
		offs = idx - numvis / 2;
		if(offs + numvis >= nelem) {
			offs = nelem - numvis;
		}
		if(offs < 0) {
			offs = 0;
		}
		wl->view_offs = offs;
	}

	wl->dirty = 1;
	tui_call_callback(w, TUI_ONMODIFY);
	return 0;
}

int tui_get_list_cur(struct tui_widget *w)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);
	return wl->cur;
}

int tui_list_cur_next(struct tui_widget *w)
{
	int nelem, numvis;
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	nelem = darr_size(wl->entries);

	numvis = VISLINES(wl);

	if(wl->cur + 1 >= nelem) {
		return -1;
	}

	if(++wl->cur - wl->view_offs >= numvis) {
		wl->view_offs = wl->cur - numvis + 1;
	}
	wl->dirty = 1;
	tui_call_callback(w, TUI_ONMODIFY);
	return 0;
}

int tui_list_cur_prev(struct tui_widget *w)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	if(wl->cur <= 0) {
		return -1;
	}
	if(--wl->cur < wl->view_offs) {
		wl->view_offs = wl->cur;
	}
	wl->dirty = 1;
	tui_call_callback(w, TUI_ONMODIFY);
	return 0;
}

int tui_list_cur_start(struct tui_widget *w)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	wl->cur = 0;
	wl->view_offs = 0;
	wl->dirty = 1;
	tui_call_callback(w, TUI_ONMODIFY);
	return 0;
}

int tui_list_cur_end(struct tui_widget *w)
{
	int nelem, numvis;
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	nelem = darr_size(wl->entries);
	numvis = VISLINES(wl);

	wl->cur = nelem - 1;
	wl->view_offs = nelem - numvis;
	if(wl->view_offs < 0) wl->view_offs = 0;
	wl->dirty = 1;
	tui_call_callback(w, TUI_ONMODIFY);
	return 0;
}

int tui_list_toggle_sel(struct tui_widget *w, int idx)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	if((wl->entries[idx].sel ^= 1)) {
		wl->num_sel++;
	} else {
		if(wl->num_sel > 0) {
			wl->num_sel--;
		}
	}
	wl->dirty = 1;
	return wl->entries[idx].sel;
}

int tui_list_is_sel(struct tui_widget *w, int idx)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	return wl->entries[idx].sel;
}

int tui_list_num_sel(struct tui_widget *w)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	return wl->num_sel;
}

int tui_list_get_sel(struct tui_widget *w, int *selv, int maxsel)
{
	int i, nitems;
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	nitems = darr_size(wl->entries);
	for(i=0; i<nitems; i++) {
		if(maxsel <= 0) break;
		if(wl->entries[i].sel) {
			*selv++ = i;
			maxsel--;
		}
	}

	return wl->num_sel;
}

void tui_sort_list(struct tui_widget *w, int (*cmpfunc)(const void*, const void*))
{
	int nelem;
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);

	if(!cmpfunc) {
		cmpfunc = (int (*)(const void*, const void*))strcmp;
	}

	nelem = darr_size(wl->entries);
	qsort(wl->entries, nelem, sizeof *wl->entries, cmpfunc);

	wl->dirty = 1;
	tui_call_callback(w, TUI_ONMODIFY);
}

static void draw_list(struct tui_widget *w, void *cls)
{
	int i, x, y, num, idx, maxlen, issel;
	struct tui_list *wl = (struct tui_list*)w;
	char *text;

	maxlen = w->width - 2;
	text = alloca(maxlen + 1);

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
		issel = wl->entries[idx].sel;

		strncpy(text, wl->entries[idx].text, maxlen);
		text[maxlen] = 0;

		if(w->focus && idx == wl->cur) {
			tg_bgcolor(TGFX_CYAN);
			tg_fgcolor(issel ? TGFX_YELLOW : TGFX_BLUE);

			tg_rect(0, x, ++y, maxlen, 1, 0);
			tg_text(x, y, "%s", text);

			tg_bgcolor(TGFX_BLUE);
			tg_fgcolor(issel ? TGFX_YELLOW : TGFX_CYAN);
		} else {
			tg_fgcolor(issel ? TGFX_YELLOW : TGFX_CYAN);
			tg_text(x, ++y, "%s", text);
		}
	}
}
