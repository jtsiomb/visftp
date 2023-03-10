#ifndef TGFX_H_
#define TGFX_H_

#include <stdarg.h>

enum {
	TGFX_BLACK,
	TGFX_BLUE,
	TGFX_GREEN,
	TGFX_CYAN,
	TGFX_RED,
	TGFX_MAGENTA,
	TGFX_YELLOW,
	TGFX_WHITE
};

/* graphics characters */
enum {
	TGFX_LARROW = 256,
	TGFX_RARROW
};

enum {
	TGFX_FRAME		= 1,
	TGFX_SHADOW		= 2
};

void tg_init(void);
void tg_cleanup(void);

void tg_redraw(void);

void tg_clear(void);

void tg_fgcolor(int col);
void tg_bgcolor(int col);
void tg_color(int col);
void tg_bgchar(int c);

void tg_setcursor(int x, int y);

void tg_putchar(int x, int y, int c);
void tg_text(int x, int y, const char *fmt, ...);
void tg_vtext(int x, int y, const char *fmt, va_list ap);

void tg_rect(const char *label, int x, int y, int xsz, int ysz, unsigned int flags);

int tg_gchar(int gchar);

#endif	/* TGFX_H_ */
