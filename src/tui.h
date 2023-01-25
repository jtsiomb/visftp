#ifndef TUI_H_
#define TUI_H_

#include <stdarg.h>

enum { TUI_INFO, TUI_WARN, TUI_ERROR };

struct tui_widget;
typedef void (*tui_callback)(struct tui_widget*, void*);

/* widget types */
enum { TUI_UNKNOWN, TUI_WINDOW, TUI_BUTTON, TUI_LIST };
/* callback types */
enum {
	TUI_ONCLICK,
	TUI_ONMODIFY,
	TUI_ONFOCUS,
	TUI_NUM_CALLBACKS
};

int tui_init(void);
void tui_shutdown(void);

struct tui_widget *tui_widget(int type);
void tui_free(struct tui_widget *w);

void tui_add_widget(struct tui_widget *par, struct tui_widget *w);
void tui_remove_widget(struct tui_widget *par, struct tui_widget *w);
struct tui_widget *tui_parent(struct tui_widget *w);

int tui_isdirty(struct tui_widget *w);
void tui_draw(struct tui_widget *w);

void tui_set_callback(struct tui_widget *w, int type, tui_callback func, void *cls);
void tui_call_callback(struct tui_widget *w, int type);

int tui_set_title(struct tui_widget *w, const char *s);
const char *tui_get_title(struct tui_widget *w);

void tui_focus(struct tui_widget *w, int focus);
int tui_hasfocus(struct tui_widget *w);

struct tui_widget *tui_window(const char *title, int x, int y, int w, int h);
struct tui_widget *tui_button(const char *title, int x, int y, tui_callback cbfunc, void *cbdata);
struct tui_widget *tui_list(const char *title, int x, int y, int w, int h, tui_callback cbfunc, void *cbdata);

void tui_clear_list(struct tui_widget *w);
void tui_add_list_item(struct tui_widget *w, const char *text);
int tui_num_list_items(struct tui_widget *w);

int tui_list_select(struct tui_widget *w, int idx);
int tui_get_list_sel(struct tui_widget *w);
int tui_list_sel_next(struct tui_widget *w);
int tui_list_sel_prev(struct tui_widget *w);
int tui_list_sel_start(struct tui_widget *w);
int tui_list_sel_end(struct tui_widget *w);

void tui_sort_list(struct tui_widget *w, int (*cmpfunc)(const void*, const void*));

void tui_wtoscr(struct tui_widget *w, int x, int y, int *retx, int *rety);
void tui_scrtow(struct tui_widget *w, int x, int y, int *retx, int *rety);

void tui_status(int type, const char *fmt, ...);
void tui_vstatus(int type, const char *fmt, va_list ap);
void tui_msgbox(int type, const char *title, const char *msg, ...);
void tui_vmsgbox(int type, const char *title, const char *msg, va_list ap);


#endif	/* TUI_H_ */
