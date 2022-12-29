#ifndef TGFX_H_
#define TGFX_H_

enum {
	TGFX_FRAME		= 1,
	TGFX_SHADOW		= 2
};

void tg_clear(void);

void tg_fgcolor(int col);
void tg_bgcolor(int col);
void tg_color(int col);
void tg_bgchar(int c);

void tg_setcursor(int x, int y);

void tg_printf(int x, int y, const char *fmt, ...);

void tg_rect(const char *label, int x, int y, int xsz, int ysz, unsigned int flags);

#endif	/* TGFX_H_ */
