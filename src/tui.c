#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tui.h"
#include "tuipriv.h"
#include "tgfx.h"
#include "darray.h"
#include "util.h"


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
		if(w->cbfunc[TUI_FREE]) {
			w->cbfunc[TUI_FREE](w, 0);
		}
		free(w->title);
		free(w);
	}
}

void tui_add_widget(struct tui_widget *par, struct tui_widget *w)
{
	if(w->par == par) return;

	w->next = par->child;
	par->child = w;
	w->par = par;
}

void tui_remove_widget(struct tui_widget *par, struct tui_widget *w)
{
	struct tui_widget *iter, dummy;

	if(w->par != par) {
		fprintf(stderr, "failed to remove widget %p from %p\n", (void*)w, (void*)par);
		return;
	}

	dummy.next = par->child;
	iter = &dummy;
	while(iter->next) {
		if(iter->next == w) {
			iter->next = w->next;
			break;
		}
		iter = iter->next;
	}
	par->child = dummy.next;

	w->next = 0;
	w->par = 0;
}

struct tui_widget *tui_parent(struct tui_widget *w)
{
	return w->par;
}

void tui_invalidate(struct tui_widget *w)
{
	w->dirty = 1;
}

int tui_isdirty(struct tui_widget *w)
{
	return w->dirty;
}

void tui_draw(struct tui_widget *w)
{
	struct tui_widget *iter;

	if(w->cbfunc[TUI_DRAW]) {
		w->cbfunc[TUI_DRAW](w, 0);
	}
	w->dirty = 0;

	iter = w->child;
	while(iter) {
		tui_draw(iter);
		iter = iter->next;
	}

	tg_redraw();
}

void tui_set_callback(struct tui_widget *w, int type, tui_callback func, void *cls)
{
	w->cbfunc[type] = func;
	w->cbcls[type] = cls;
}

void tui_call_callback(struct tui_widget *w, int type)
{
	if(w->cbfunc[type]) {
		w->cbfunc[type](w, w->cbcls[type]);
	}
}

void tui_focus(struct tui_widget *w, int focus)
{
	focus = focus ? 1 : 0;
	if(w->focus == focus) {
		return;
	}
	w->focus = focus;
	w->dirty = 1;
	tui_call_callback(w, TUI_ONFOCUS);
}

int tui_set_title(struct tui_widget *w, const char *s)
{
	free(w->title);
	w->title = strdup_nf(s);
	return 0;
}

const char *tui_get_title(struct tui_widget *w)
{
	return w->title;
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

void tui_wtoscr(struct tui_widget *w, int x, int y, int *retx, int *rety)
{
	while(w) {
		x += w->x;
		y += w->y;
		w = w->par;
	}
	*retx = x;
	*rety = y;
}

void tui_scrtow(struct tui_widget *w, int x, int y, int *retx, int *rety)
{
	while(w) {
		x -= w->x;
		y -= w->y;
		w = w->par;
	}
	*retx = x;
	*rety = y;
}


void tui_status(int type, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	tui_vstatus(type, fmt, ap);
	va_end(ap);
}

void tui_vstatus(int type, const char *fmt, va_list ap)
{
	/* TODO */
}

void tui_msgbox(int type, const char *title, const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	tui_vmsgbox(type, title, msg, ap);
	va_end(ap);
}

void tui_vmsgbox(int type, const char *title, const char *msg, va_list ap)
{
	/* TODO */
}
