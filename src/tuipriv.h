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

struct tui_list {
	WCOMMON;
	char **entries;	/* darr */
};
	

#endif	/* TUIPRIV_H_ */
