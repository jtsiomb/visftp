#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <dos.h>
#include <conio.h>
#include "tgfx.h"

static unsigned short attr = 0x0700;
static int bgcol = 0, fgcol = 7;
static int bgchar = ' ';
static unsigned short *framebuf = (unsigned short*)0xb8000;

#define UPD_ATTR	attr = (fgcol << 8) | (bgcol << 12)

void tg_init(void)
{
}

void tg_cleanup(void)
{
	int i;
	for(i=0; i<80*25; i++) {
		framebuf[i] = 0x0720;
	}
	tg_setcursor(0, 0);
}

void tg_redraw(void)
{
}

void tg_clear(void)
{
	tg_rect(0, 0, 0, 80, 25, 0);
}

void tg_fgcolor(int col)
{
	fgcol = col & 0xf;
	UPD_ATTR;
}

void tg_bgcolor(int col)
{
	bgcol = col & 0xf;
	UPD_ATTR;
}

void tg_color(int col)
{
	fgcol = col & 0xf;
	bgcol = (col >> 4) & 0xf;
	attr = col << 8;
}

void tg_bgchar(int c)
{
	bgchar = c;
}

#define CRTC_ADDR_PORT	0x3d4
#define CRTC_DATA_PORT	0x3d5
#define REG_CRTC_CURH	0xe
#define REG_CRTC_CURL	0xf

void tg_setcursor(int x, int y)
{
	unsigned int addr = y * 80 + x;

	outpw(CRTC_ADDR_PORT, (addr & 0xff00) | REG_CRTC_CURH);
	outpw(CRTC_ADDR_PORT, (addr << 8) | REG_CRTC_CURL);
}

void tg_putchar(int x, int y, int c)
{
	static int prev_y = -1;
	static unsigned short *lineptr;

	if(y != prev_y) {
		lineptr = framebuf + y * 80;
		prev_y = y;
	}
	lineptr[x] = c | attr;
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
	char buf[256], *ptr;
	unsigned short *fbptr = framebuf + y * 80 + x;

	vsprintf(buf, fmt, ap);

	ptr = buf;
	while(*ptr) {
		*fbptr++ = *ptr++ | attr;
	}
}

void tg_rect(const char *label, int x, int y, int xsz, int ysz, unsigned int flags)
{
	int i, j, len, maxlen;
	unsigned short *fbptr = framebuf + y * 80 + x;

	for(i=0; i<ysz; i++) {
		for(j=0; j<xsz; j++) {
			fbptr[j] = attr | bgchar;
		}
		fbptr += 80;
	}

	if((flags & TGFX_FRAME) && xsz >= 2 && ysz >= 2) {
		fbptr = framebuf + y * 80 + x;
		for(i=0; i<xsz-2; i++) {
			fbptr[i + 1] = attr | 0xcd;
			fbptr[(ysz-1) * 80 + i + 1] = attr | 0xcd;
		}
		for(i=0; i<ysz-2; i++) {
			fbptr[(i + 1) * 80] = attr | 0xba;
			fbptr[(xsz-1) + (i + 1) * 80] = attr | 0xba;
		}
		fbptr[0] = attr | 0xc9;
		fbptr[xsz-1] = attr | 0xbb;
		fbptr += (ysz - 1) * 80;
		fbptr[0] = attr | 0xc8;
		fbptr[xsz-1] = attr | 0xbc;
	}

	if(label && xsz >= 8) {
		maxlen = xsz - 4;
		if((len = strlen(label)) > maxlen) {
			label += len - maxlen;
		}
		tg_text(x + 2, y, "%s", label);
	}
}

int tg_gchar(int gchar)
{
	switch(gchar) {
	case TGFX_LARROW:
		return 0x1b;
	case TGFX_RARROW:
		return 0x1a;
	default:
		break;
	}
	return '@';
}
