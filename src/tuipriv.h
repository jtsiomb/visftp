#ifndef TUIPRIV_H_
#define TUIPRIV_H_

#include "tui.h"

#define WCOMMON	\
	int type; \
	char *title; \
	int x, y, width, height; \
	tui_callback cbfunc[TUI_NUM_CALLBACKS]; \
	void *cbcls[TUI_NUM_CALLBACKS]; \
	struct widget *next

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
