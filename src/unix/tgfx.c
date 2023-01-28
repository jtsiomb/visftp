#include <string.h>
#include <curses.h>
#include "tgfx.h"
#include "util.h"

struct cpair {
	int pair;
	int fg, bg;
};
static struct cpair *colors;
static int num_colors;

static int fgcol, bgcol;
static int bgchar;
static int cur_pair;
static int cur_x, cur_y;

static int curses_color(int col);
static void upd_color(void);

void tg_init(void)
{
	initscr();
	cbreak();
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	noecho();
	curs_set(0);
	start_color();

	fgcol = curses_color(TGFX_WHITE);
	bgcol = curses_color(TGFX_BLACK);
	bgchar = ' ';

	colors = malloc_nf(COLORS * sizeof *colors);
	num_colors = 0;
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
	clear();
}


void tg_fgcolor(int col)
{
	fgcol = curses_color(col);
}

void tg_bgcolor(int col)
{
	bgcol = curses_color(col);
}

void tg_color(int col)
{
	fgcol = curses_color(col & 0xf);
	bgcol = curses_color((col >> 4) & 0xf);
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
	upd_color();
	attron(COLOR_PAIR(cur_pair));
	move(y, x);
	vw_printw(stdscr, fmt, ap);
	attroff(COLOR_PAIR(cur_pair));
}


void tg_rect(const char *label, int x, int y, int xsz, int ysz, unsigned int flags)
{
	int i, len, maxlen;

	upd_color();
	attron(COLOR_PAIR(cur_pair));

	for(i=0; i<ysz; i++) {
		move(y + i, x);
		hline(bgchar, xsz);
	}

	if((flags & TGFX_FRAME) && xsz >= 2 && ysz >= 2) {
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

	if(label && xsz >= 8) {
		maxlen = xsz - 4;
		if((len = strlen(label)) > maxlen) {
			label += len - maxlen;
		}
		tg_text(x + 2, y, "%s", label);
	}

	attroff(COLOR_PAIR(cur_pair));
	refresh();
}

int tg_gchar(int gchar)
{
	switch(gchar) {
	case TGFX_LARROW:
		return ACS_LARROW;
	case TGFX_RARROW:
		return ACS_RARROW;
	default:
		break;
	}
	return '@';
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

static void upd_color(void)
{
	int i;

	for(i=0; i<num_colors; i++) {
		if(fgcol == colors[i].fg && bgcol == colors[i].bg) {
			cur_pair = colors[i].pair;
			return;
		}
	}

	/* not found, allocate a new color pair */
	if(num_colors >= COLORS) {
		return;
	}
	i = num_colors++;
	cur_pair = num_colors;

	colors[i].fg = fgcol;
	colors[i].bg = bgcol;
	colors[i].pair = cur_pair;
	init_pair(cur_pair, fgcol, bgcol);
}
