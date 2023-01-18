#include <curses.h>
#include "tgfx.h"

static int fgcol, bgcol;
static int bgchar;
static int cur_x, cur_y;

static int curses_color(int col);

void tg_init(void)
{
	initscr();
	raw();
	keypad(stdscr, TRUE);
	noecho();
	start_color();

	fgcol = curses_color(TGFX_WHITE);
	bgcol = curses_color(TGFX_BLACK);
	bgchar = ' ';

	tg_color(fgcol | (bgcol << 4));
}

void tg_cleanup(void)
{
	endwin();
}

void tg_redraw(void)
{
	move(cur_y, cur_x);
	refresh();
}

void tg_clear(void)
{
}


void tg_fgcolor(int col)
{
	fgcol = curses_color(col);
	init_pair(1, fgcol, bgcol);
}

void tg_bgcolor(int col)
{
	bgcol = curses_color(col);
	init_pair(1, fgcol, bgcol);
}

void tg_color(int col)
{
	fgcol = curses_color(col & 0xf);
	bgcol = curses_color((col >> 4) & 0xf);
	init_pair(1, fgcol, bgcol);
}

void tg_bgchar(int c)
{
	bgchar = c;
}


void tg_setcursor(int x, int y)
{
	move(y, x);
	cur_x = x;
	cur_y = y;
}


void tg_text(int x, int y, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	tg_vtext(x, y, fmt, ap);
	va_end(ap);
}

void tg_vtext(int x, int y, const char *fmt, va_list ap)
{
	attron(COLOR_PAIR(1));
	move(y, x);
	vw_printw(stdscr, fmt, ap);
	attroff(COLOR_PAIR(1));
}


void tg_rect(const char *label, int x, int y, int xsz, int ysz, unsigned int flags)
{
	int i;

	attron(COLOR_PAIR(1));

	for(i=0; i<ysz; i++) {
		move(y + i, x);
		hline(bgchar, xsz);
	}

	if(flags & TGFX_FRAME) {
		move(y, x + 1);
		hline(ACS_HLINE, xsz - 2);
		move(y + ysz - 1, x + 1);
		hline(ACS_HLINE, xsz - 2);
		move(y + 1, x);
		vline(ACS_VLINE, ysz - 2);
		move(y + 1, x + xsz - 1);
		vline(ACS_VLINE, ysz - 2);

		mvaddch(y, x, ACS_ULCORNER);
		mvaddch(y, x + xsz - 1, ACS_URCORNER);
		mvaddch(y + ysz - 1, x, ACS_LLCORNER);
		mvaddch(y + ysz - 1, x + xsz - 1, ACS_LRCORNER);
	}

	if(label) {
		tg_text(x + 2, y, "%s", label);
	}

	attroff(COLOR_PAIR(1));
}

static int curses_color(int col)
{
	switch(col) {
	case TGFX_RED:
		return COLOR_RED;
	case TGFX_BLUE:
		return COLOR_BLUE;
	case TGFX_CYAN:
		return COLOR_CYAN;
	case TGFX_YELLOW:
		return COLOR_YELLOW;
	default:
		break;
	}
	return col;
}
