#ifndef TUI_H_
#define TUI_H_

struct tui_widget;
typedef void (*tui_callback)(struct tui_widget*);

/* widget types */
enum { TUI_UNKNOWN, TUI_WINDOW, TUI_BUTTON, TUI_LIST };
/* callback types */
enum {
	TUI_ONCLICK,
	TUI_ONMODIFY,
	TUI_NUM_CALLBACKS
};

int tui_init(void);
void tui_shutdown(void);

struct tui_widget *tui_widget(int type);
void tui_free(struct tui_widget *w);

void tui_set_callback(struct tui_widget *w, int type, tui_callback func, void *cls);

struct tui_widget *tui_window(const char *title, int x, int y, int w, int h);
struct tui_widget *tui_button(const char *title, int x, int y, tui_callback cbfunc, void *cbdata);
struct tui_widget *tui_list(const char *title, int x, int y, int w, int h, tui_callback cbfunc, void *cbdata);

void tui_clear_list(struct tui_widget *w);
void tui_add_list_item(struct tui_widget *w, const char *text);


#endif	/* TUI_H_ */
