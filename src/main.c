#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <sys/select.h>
#include "tgfx.h"
#include "input.h"
#include "util.h"
#include "tui.h"
#include "ftp.h"

void updateui(void);
int proc_input(void);
int keypress(int key);
int parse_args(int argc, char **argv);

static struct ftp *ftp;
static struct tui_widget *uilist;

static char *host = "localhost";
static int port = 21;

int main(int argc, char **argv)
{
	int i, numsock, maxfd;
	int ftpsock[16];
	fd_set rdset;
	struct timeval tv;

	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	if(!(ftp = ftp_alloc())) {
		return 1;
	}
	if(ftp_connect(ftp, host, port) == -1) {
		ftp_free(ftp);
		return 1;
	}

	init_input();

	tg_init();

	tg_bgchar(' ');
	tg_clear();

	uilist = tui_list("Remote", 0, 0, 40, 23, 0, 0);
	tui_add_list_item(uilist, "first item");
	tui_add_list_item(uilist, "second item");
	tui_add_list_item(uilist, "another item");
	tui_add_list_item(uilist, "foo");

	tg_setcursor(0, 23);

	tui_draw(uilist);

	for(;;) {
		FD_ZERO(&rdset);
		maxfd = 0;

		numsock = ftp_sockets(ftp, ftpsock, sizeof ftpsock);
		for(i=0; i<numsock; i++) {
			FD_SET(ftpsock[i], &rdset);
			if(ftpsock[i] > maxfd) maxfd = ftpsock[i];
		}

#ifdef __unix__
		FD_SET(0, &rdset);
		tv.tv_sec = 120;
		tv.tv_usec = 0;
#else
		tv.tv_sec = tv.tv_usec = 0;
#endif

		if(select(maxfd + 1, &rdset, 0, 0, &tv) == -1 && errno == EINTR) {
			continue;
		}

#ifdef __unix__
		if(FD_ISSET(0, &rdset)) {
			if(proc_input() == -1) {
				break;
			}
		}
#endif

		for(i=0; i<numsock; i++) {
			if(FD_ISSET(ftpsock[i], &rdset)) {
				ftp_handle(ftp, ftpsock[i]);
			}
		}

		if(ftp->modified) {
			updateui();
			ftp->modified = 0;
		}
	}

	tg_cleanup();
	cleanup_input();
	ftp_close(ftp);
	ftp_free(ftp);
	return 0;
}

void updateui(void)
{
	unsigned int upd = 0;

	if(ftp->curdir_rem && strcmp(tui_get_title(uilist), ftp->curdir_rem) != 0) {
		tui_set_title(uilist, ftp->curdir_rem);
		upd |= 1;
	}

	if(upd & 1) {
		tui_draw(uilist);
	}
}

int proc_input(void)
{
	union event ev;

	while(poll_input(&ev)) {
		switch(ev.type) {
		case EV_KEY:
			if(keypress(ev.key.key) == -1) {
				return -1;
			}
			break;

		default:
			break;
		}
	}
	return 0;
}

int keypress(int key)
{
	switch(key) {
	case 27:
	case 'q':
		return -1;

	default:
		break;
	}
	return 0;
}

static const char *usage = "Usage: %s [options] [hostname] [port]\n"
	"Options:\n"
	"  -h: print usage information and exit\n";

int parse_args(int argc, char **argv)
{
	int i, argidx = 0;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(argv[i][2] != 0) {
				goto inval;
			}
			switch(argv[i][1]) {
			case 'h':
				printf(usage, argv[0]);
				exit(0);

			default:
				goto inval;
			}

		} else {
			switch(argidx++) {
			case 0:
				host = argv[i];
				break;

			case 1:
				if((port = atoi(argv[i])) <= 0) {
					fprintf(stderr, "invalid port number: %s\n", argv[i]);
					return -1;
				}
				break;

			default:
				goto inval;
			}
		}
	}

	return 0;
inval:
	fprintf(stderr, "invalid argument: %s\n", argv[i]);
	fprintf(stderr, usage, argv[0]);
	return -1;
}
