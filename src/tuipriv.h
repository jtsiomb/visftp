#ifndef TUIPRIV_H_
#define TUIPRIV_H_

#include "tui.h"

enum {
	TUI_DRAW = TUI_NUM_CALLBACKS,
	TUI_FREE
};

#define WCOMMON	\
	int type; \
	char *title; \
	int x, y, width, height; \
	int focus, dirty; \
	tui_callback cbfunc[TUI_NUM_CALLBACKS]; \
	void *cbcls[TUI_NUM_CALLBACKS]; \
	struct tui_widget *par, *child, *next

struct tui_widget {
	WCOMMON;
};

struct tui_list_entry {
	char *text;
	void *data;
};

struct list_entry {
	char *text;
	int sel;
};

struct tui_list {
	WCOMMON;
	struct list_entry *entries;	/* darr */
	int num_sel;
	int cur;
	int view_offs;
};


#endif	/* TUIPRIV_H_ */
