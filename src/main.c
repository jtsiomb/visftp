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

		updateui();
	}

	tg_cleanup();
	cleanup_input();
	ftp_close(ftp);
	ftp_free(ftp);
	return 0;
}

void updateui(void)
{
	int i, num;
	struct ftp_dirent *ent;
	unsigned int upd = 0;
	char buf[128];
	const char *remdir;

	remdir = ftp_curdir(ftp, FTP_REMOTE);
	if(remdir && strcmp(tui_get_title(uilist), remdir) != 0) {
		tui_set_title(uilist, remdir);
		upd |= 1;
	}

	if(ftp->modified & FTP_MOD_REMDIR) {
		tui_clear_list(uilist);

		num = ftp_num_dirent(ftp, FTP_REMOTE);
		for(i=0; i<num; i++) {
			ent = ftp_dirent(ftp, FTP_REMOTE, i);
			if(ent->type == FTP_DIR) {
				sprintf(buf, "%s/", ent->name);
				tui_add_list_item(uilist, buf);
			} else {
				tui_add_list_item(uilist, ent->name);
			}
		}

		tui_list_select(uilist, 0);

		ftp->modified &= ~FTP_MOD_REMDIR;
		upd |= 1;
	}

	if(tui_isdirty(uilist) || upd & 1) {
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
	int sel;
	const char *name;

	switch(key) {
	case 27:
	case 'q':
		return -1;

	case KB_UP:
		tui_list_sel_prev(uilist);
		break;
	case KB_DOWN:
		tui_list_sel_next(uilist);
		break;
	case KB_LEFT:
		tui_list_sel_start(uilist);
		break;
	case KB_RIGHT:
		tui_list_sel_end(uilist);
		break;

	case '\n':
		sel = tui_get_list_sel(uilist);
		name = ftp_dirent(ftp, FTP_REMOTE, sel)->name;
		ftp_queue(ftp, FTP_CHDIR, name);
		break;

	case '\b':
		infomsg("CDUP\n");
		ftp_queue(ftp, FTP_CDUP, 0);
		break;

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
