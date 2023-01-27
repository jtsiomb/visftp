#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#ifdef __DOS__
#include <direct.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif
#include <sys/stat.h>
#include <sys/select.h>
#include "tgfx.h"
#include "input.h"
#include "util.h"
#include "tui.h"
#include "ftp.h"
#include "darray.h"

#ifdef __DOS__
#define select	select_s
#endif

void updateui(void);
int update_localdir(void);
int proc_input(void);
int keypress(int key);
static void fixname(char *dest, const char *src);
static void xfer_done(struct ftp *ftp, struct ftp_transfer *xfer);
int parse_args(int argc, char **argv);

static struct ftp *ftp;
static struct tui_widget *uilist[2];

static int focus;

static char *host = "localhost";
static int port = 21;

static char curdir[PATH_MAX + 1];
static struct ftp_dirent *localdir;
static int local_modified;


int main(int argc, char **argv)
{
	int i, numsock, maxfd;
	int ftpsock[16];
	fd_set rdset;
	struct timeval tv;

	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	localdir = darr_alloc(0, sizeof *localdir);
	getcwd(curdir, sizeof curdir);
	update_localdir();

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

	uilist[0] = tui_list("Remote", 0, 0, 40, 23, 0, 0);
	uilist[1] = tui_list("Local", 40, 0, 40, 23, 0, 0);
	focus = 0;
	tui_focus(uilist[focus], 1);

	tg_setcursor(0, 23);

	tui_draw(uilist[0]);
	tui_draw(uilist[1]);

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
#else
		if(proc_input() == -1) {
			break;
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

	remdir = ftp_curdir(ftp);
	if(remdir && strcmp(tui_get_title(uilist[0]), remdir) != 0) {
		tui_set_title(uilist[0], remdir);
		upd |= 1;
	}

	if(ftp->modified & FTP_MOD_DIR) {
		tui_clear_list(uilist[0]);

		num = ftp_num_dirent(ftp);
		for(i=0; i<num; i++) {
			ent = ftp_dirent(ftp, i);
			if(ent->type == FTP_DIR) {
				sprintf(buf, "%s/", ent->name);
				tui_add_list_item(uilist[0], buf);
			} else {
				tui_add_list_item(uilist[0], ent->name);
			}
		}

		tui_list_select(uilist[0], 0);

		ftp->modified &= ~FTP_MOD_DIR;
		upd |= 1;
	}

	if(local_modified || strcmp(tui_get_title(uilist[1]), curdir) != 0) {
		tui_clear_list(uilist[1]);
		num = darr_size(localdir);
		for(i=0; i<num; i++) {
			ent = localdir + i;
			if(ent->type == FTP_DIR) {
				sprintf(buf, "%s/", ent->name);
				tui_add_list_item(uilist[1], buf);
			} else {
				tui_add_list_item(uilist[1], ent->name);
			}
		}
		tui_list_select(uilist[1], 0);
		tui_set_title(uilist[1], curdir);

		local_modified = 0;
		upd |= 2;
	}

	if(tui_isdirty(uilist[0]) || upd & 1) {
		tui_draw(uilist[0]);
	}
	if(tui_isdirty(uilist[1]) || upd & 2) {
		tui_draw(uilist[1]);
	}
}

int update_localdir(void)
{
	DIR *dir;
	struct dirent *dent;
	struct stat st;
	struct ftp_dirent ent;

	if(!(dir = opendir(curdir))) {
		errmsg("failed to open directory: %s\n", curdir);
		return -1;
	}

	darr_clear(localdir);
	while((dent = readdir(dir))) {
		ent.name = strdup_nf(dent->d_name);
		if(strcmp(dent->d_name, ".") == 0) continue;

		if(stat(dent->d_name, &st) == 0) {
			if(S_ISDIR(st.st_mode)) {
				ent.type = FTP_DIR;
			} else {
				ent.type = FTP_FILE;
			}
			ent.size = st.st_size;
		} else {
			ent.type = FTP_FILE;
			ent.size = 0;
		}

		darr_push(localdir, &ent);
	}
	closedir(dir);

	qsort(localdir, darr_size(localdir), sizeof *localdir, ftp_direntcmp);
	local_modified = 1;
	return 0;
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

	switch(key) {
	case 27:
	case 'q':
	case KB_F10:
		return -1;

	case '\t':
		tui_focus(uilist[focus], 0);
		focus ^= 1;
		tui_focus(uilist[focus], 1);
		break;

	case KB_UP:
		tui_list_sel_prev(uilist[focus]);
		break;
	case KB_DOWN:
		tui_list_sel_next(uilist[focus]);
		break;
	case KB_LEFT:
		tui_list_sel_start(uilist[focus]);
		break;
	case KB_RIGHT:
		tui_list_sel_end(uilist[focus]);
		break;

	case '\n':
		sel = tui_get_list_sel(uilist[focus]);
		if(focus == 0) {
			struct ftp_dirent *ent = ftp_dirent(ftp, sel);
			if(ent->type == FTP_DIR) {
				ftp_queue(ftp, FTP_CHDIR, ent->name);
			} else {
				/* TODO */
			}
		} else {
			if(localdir[sel].type == FTP_DIR) {
				if(chdir(localdir[sel].name) == -1) {
					errmsg("failed to change directory: %s\n", localdir[sel].name);
				} else {
					getcwd(curdir, sizeof curdir);
					update_localdir();
				}
			} else {
				/* TODO */
			}
		}
		break;

	case '\b':
		if(focus == 0) {
			ftp_queue(ftp, FTP_CDUP, 0);
		} else {
			if(chdir("..") == 0) {
				getcwd(curdir, sizeof curdir);
				update_localdir();
			}
		}
		break;

	case KB_F5:
		sel = tui_get_list_sel(uilist[focus]);
		if(focus == 0) {
			struct ftp_transfer *xfer;
			struct ftp_dirent *ent = ftp_dirent(ftp, sel);
			char *lname = alloca(strlen(ent->name) + 1);

			fixname(lname, ent->name);

			xfer = malloc_nf(sizeof *xfer);
			if(!(xfer->fp = fopen(lname, "wb"))) {
				errmsg("failed to open %s: %s\n", lname, strerror(errno));
				free(xfer);
				break;
			}

			xfer->op = FTP_RETR;
			xfer->rname = strdup_nf(ent->name);
			xfer->total = ent->size;
			xfer->done = xfer_done;

			ftp_queue_transfer(ftp, xfer);
			local_modified = 1;
		} else {
			/* TODO */
		}
		break;

	default:
		break;
	}
	return 0;
}

static void fixname(char *dest, const char *src)
{
	strcpy(dest, src);

#ifdef __DOS__
	{
		char *suffix;
		if((suffix = strrchr(dest, '.'))) {
			*suffix++ = 0;
			if(strlen(suffix) > 3) {
				suffix[3] = 0;
			}
		}
		if(strlen(dest) > 8) {
			dest[8] = 0;
		}

		if(suffix) {
			strcat(dest, ".");
			strcat(dest, suffix);
		}
	}
#endif
}

static void xfer_done(struct ftp *ftp, struct ftp_transfer *xfer)
{
	if(xfer->fp) {
		fclose(xfer->fp);
	}
	free(xfer->rname);
	free(xfer);
	update_localdir();
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
