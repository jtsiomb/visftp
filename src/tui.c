#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tui.h"
#include "tuipriv.h"
#include "tgfx.h"
#include "darray.h"


int tui_init(void)
{
	return 0;
}

void tui_shutdown(void)
{
}


struct tui_widget *tui_widget(int type)
{
	struct tui_widget *w;

	if(!(w = calloc(1, sizeof *w))) {
		return 0;
	}
	w->type = type;
	return w;
}

void tui_free(struct tui_widget *w)
{
	if(w) {
		free(w->title);
		free(w);
	}
}

void tui_set_callback(struct tui_widget *w, int type, tui_callback func, void *cls)
{
	w->cbfunc[type] = func;
	w->cbcls[type] = cls;
}

struct tui_widget *tui_window(const char *title, int x, int y, int width, int height)
{
	struct tui_widget *w;

	if(!(w = tui_widget(TUI_WINDOW))) {
		return 0;
	}
	w->title = strdup(title);
	w->x = x;
	w->y = y;
	w->width = width;
	w->height = height;
	return w;
}

struct tui_widget *tui_button(const char *title, int x, int y, tui_callback cbfunc, void *cbdata)
{
	struct tui_widget *w;

	if(!(w = tui_widget(TUI_BUTTON))) {
		return 0;
	}
	w->title = strdup(title);
	w->x = x;
	w->y = y;
	w->width = strlen(title) + 2;
	w->height = 1;

	if(cbfunc) {
		tui_set_callback(w, TUI_ONCLICK, cbfunc, cbdata);
	}
	return w;
}

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

	if(cbfunc) {
		tui_set_callback((struct tui_widget*)w, TUI_ONMODIFY, cbfunc, cbdata);
	}
	return (struct tui_widget*)w;
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
