#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "darray.h"
#include "tui.h"
#include "tuipriv.h"
#include "tgfx.h"

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

	if(cbfunc) {
		tui_set_callback((struct tui_widget*)w, TUI_ONMODIFY, cbfunc, cbdata);
	}
	tui_set_callback((struct tui_widget*)w, TUI_FREE, free_list, 0);
	tui_set_callback((struct tui_widget*)w, TUI_DRAW, draw_list, 0);
	return (struct tui_widget*)w;
}

static void free_list(struct tui_widget *w, void *cls)
{
	darr_free(((struct tui_list*)w)->entries);
}


void tui_clear_list(struct tui_widget *w)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);
	darr_clear(wl->entries);
}

void tui_add_list_item(struct tui_widget *w, const char *text)
{
	struct tui_list *wl = (struct tui_list*)w;
	assert(wl->type == TUI_LIST);
	darr_push(wl->entries, &text);
}

static void draw_list(struct tui_widget *w, void *cls)
{
	int i, x, y, num;
	struct tui_list *wl = (struct tui_list*)w;

	tui_wtoscr(w, 0, 0, &x, &y);

	tg_bgcolor(TGFX_BLUE);
	tg_rect(wl->title, x, y, wl->width, wl->height, TGFX_FRAME);

	num = darr_size(wl->entries);
	if(num > wl->height - 2) {
		num = wl->height - 2;
	}

	x++;
	for(i=0; i<num; i++) {
		tg_text(x, ++y, "%s", wl->entries[i]);
	}
}
