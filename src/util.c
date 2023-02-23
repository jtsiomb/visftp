#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "tui.h"

void *malloc_nf_impl(size_t sz, const char *file, int line)
{
	void *p;
	if(!(p = malloc(sz))) {
		fprintf(stderr, "%s:%d failed to allocate %lu bytes\n", file, line, (unsigned long)sz);
		abort();
	}
	return p;
}

void *calloc_nf_impl(size_t num, size_t sz, const char *file, int line)
{
	void *p;
	if(!(p = calloc(num, sz))) {
		fprintf(stderr, "%s:%d failed to allocate %lu bytes\n", file, line, (unsigned long)(sz * num));
		abort();
	}
	return p;
}

void *realloc_nf_impl(void *p, size_t sz, const char *file, int line)
{
	if(!(p = realloc(p, sz))) {
		fprintf(stderr, "%s:%d failed to realloc %lu bytes\n", file, line, (unsigned long)sz);
		abort();
	}
	return p;
}

char *strdup_nf_impl(const char *s, const char *file, int line)
{
	char *res;
	if(!(res = strdup(s))) {
		fprintf(stderr, "%s:%d failed to duplicate string\n", file, line);
		abort();
	}
	return res;
}


int match_prefix(const char *str, const char *prefix)
{
	while(*str && *prefix) {
		if(*str++ != *prefix++) {
			return 0;
		}
	}
	return *prefix ? 0 : 1;
}

static FILE *logfile;

#ifdef __DOS__
static int setup_serial(void);
static void ser_putchar(int c);
static void ser_puts(const char *s);

static int sdev = -1;

#define LOG_OPEN	(logfile || sdev >= 0)
#else
#define LOG_OPEN	(logfile)
#endif


static void closelog(void)
{
	fclose(logfile);
}

static int loginit(void)
{
	char *fname;
	char *env = getenv("VISFTP_LOG");

	if(env) {
		fname = env;
	} else {
#ifdef __DOS__
		char *tmpdir = getenv("TEMP");
		if(!tmpdir) {
			tmpdir = getenv("VISFTP");
		}
		if(tmpdir) {
			fname = alloca(strlen(tmpdir) + 16);
			sprintf(fname, "%s\\visftp.log", tmpdir);
		} else {
			fname = "c:\\visftp.log";
		}
#else
		fname = "/tmp/visftp.log";
#endif
	}

#ifdef __DOS__
	if(sscanf(fname, "COM%d", &sdev) == 1 || sscanf(fname, "com%d", &sdev) == 1) {
		sdev--;
		if(setup_serial() == -1) {
			sdev = -1;
			fprintf(stderr, "failed to setup serial port (%s)\n", fname);
			return -1;
		}
		return 0;
	}
#endif

	if(!(logfile = fopen(fname, "w"))) {
		return -1;
	}
	setvbuf(logfile, 0, _IOLBF, 0);
	atexit(closelog);
	return 0;
}

static void logmsg(const char *tag, const char *fmt, va_list ap)
{
	if(!LOG_OPEN) {
		if(loginit() == -1) {
			return;
		}
	}

#ifdef __DOS__
	if(sdev != -1) {
		char buf[512];
		sprintf(buf, "%s: ", tag);
		vsprintf(buf, fmt, ap);
		ser_puts(buf);
	} else {
#endif
		fprintf(logfile, "%s: ", tag);
		vfprintf(logfile, fmt, ap);
#ifdef __DOS__
	}
#endif
}

void errmsg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	tui_vmsgbox(TUI_ERROR, "error", fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	logmsg("error", fmt, ap);
	va_end(ap);
}

void warnmsg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	tui_status(TUI_WARN, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	logmsg("warning", fmt, ap);
	va_end(ap);
}

void infomsg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	tui_status(TUI_INFO, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	logmsg("info", fmt, ap);
	va_end(ap);
}

#ifdef __DOS__
#include <conio.h>

#define UART1_BASE		0x3f8
#define UART2_BASE		0x2f8

#define UART_DATA		0
#define UART_DIVLO		0
#define UART_DIVHI		1
#define UART_FIFO		2
#define UART_LCTL		3
#define UART_MCTL		4
#define UART_LSTAT		5

#define DIV_9600			(115200 / 9600)
#define DIV_19200			(115200 / 19200)
#define DIV_38400			(115200 / 38400)
#define LCTL_8N1			0x03
#define LCTL_DLAB			0x80
#define FIFO_ENABLE_CLEAR	0x07
#define MCTL_DTR_RTS_OUT2	0x0b
#define LST_TREG_EMPTY		0x20

static unsigned int iobase;

static int setup_serial(void)
{
	switch(sdev) {
	case 0:
		iobase = UART1_BASE;
		break;
	case 1:
		iobase = UART2_BASE;
		break;
	default:
		sdev = -1;
		return -1;
	}

	/* set clock divisor */
	outp(iobase | UART_LCTL, LCTL_DLAB);
	outp(iobase | UART_DIVLO, DIV_9600 & 0xff);
	outp(iobase | UART_DIVHI, DIV_9600 >> 8);
	/* set format 8n1 */
	outp(iobase | UART_LCTL, LCTL_8N1);
	/* clear and enable fifo */
	outp(iobase | UART_FIFO, FIFO_ENABLE_CLEAR);
	/* assert RTS and DTR */
	outp(iobase | UART_MCTL, MCTL_DTR_RTS_OUT2);
	return 0;
}

static void ser_putchar(int c)
{
	if(c == '\n') {
		ser_putchar('\r');
	}

	while((inp(iobase | UART_LSTAT) & LST_TREG_EMPTY) == 0);
	outp(iobase | UART_DATA, c);
}

static void ser_puts(const char *s)
{
	while(*s) {
		while((inp(iobase | UART_LSTAT) & LST_TREG_EMPTY) == 0);
		if(*s == '\n') {
			outp(iobase | UART_DATA, '\r');
		}
		outp(iobase | UART_DATA, *s++);
	}
}
#endif	/* __DOS__ */
