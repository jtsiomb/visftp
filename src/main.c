#include <stdio.h>
#include <string.h>
#include <ctype.h>
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
#include "viewer.h"
#include "darray.h"

#ifdef __DOS__
#define select	select_s
#endif

struct server {
	char *name;
	char *host;
	char *user;
	char *pass;
};


void updateui(void);
int update_localdir(void);
int proc_input(void);
int keypress(int key);
static void fixname(char *dest, const char *src);
static void xfer_done(struct ftp *ftp, struct ftp_transfer *xfer);
static void view_done(struct ftp *ftp, struct ftp_transfer *xfer);
int read_servers(const char *fname);
struct server *find_server(const char *name);
int parse_args(int argc, char **argv);

static struct ftp *ftp;
static struct tui_widget *uilist[2];

static int focus;

static char *host = "localhost";
static int port = 21;
static struct server *srvlist;

static char curdir[PATH_MAX + 1];
static struct ftp_dirent *localdir;
static int local_modified;
static struct ftp_transfer *cur_xfer;

#ifdef __DOS__
void detect_lfn(void);

static int have_lfn;
#endif


int main(int argc, char **argv)
{
	int i, numsock, maxfd;
	int ftpsock[16];
	fd_set rdset;
	struct timeval tv;
	struct server *srv;

#ifdef __DOS__
	detect_lfn();
#endif

	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	read_servers("visftp.srv");

	localdir = darr_alloc(0, sizeof *localdir);
	getcwd(curdir, sizeof curdir);
	update_localdir();

	if(!(ftp = ftp_alloc())) {
		return 1;
	}

	if((srv = find_server(host))) {
		ftp_auth(ftp, srv->user, srv->pass);
	}

	if(ftp_connect(ftp, srv ? srv->host : host, port) == -1) {
		ftp_free(ftp);
		return 1;
	}

	init_input();

	tg_init();

	tg_bgchar(' ');
	tg_clear();

	uilist[0] = tui_list("Remote", 0, 1, 40, 21, 0, 0);
	uilist[1] = tui_list("Local", 40, 1, 40, 21, 0, 0);
	focus = 0;
	tui_focus(uilist[focus], 1);

	local_modified = 1;

	tg_setcursor(0, 23);

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
	int i, num, progr;
	struct ftp_dirent *ent;
	unsigned int upd = 0;
	char buf[128];
	const char *remdir;
	static int prev_status;
	static void *prev_xfer;

	if(ftp->status != prev_status) {
		tg_fgcolor(ftp->status ? TGFX_GREEN : TGFX_RED);
		tg_bgcolor(TGFX_BLACK);
		tg_text(0, 0, "Srv: %s", ftp->status ? host : "-");
		upd |= 0x8000;
		prev_status = ftp->status;
	}

	tg_fgcolor(TGFX_WHITE);
	tg_bgcolor(TGFX_BLACK);
	if(cur_xfer) {
		int dir = tg_gchar(cur_xfer->op == FTP_RETR ? TGFX_RARROW : TGFX_LARROW);
		tg_text(40, 0, "%c%s", dir, cur_xfer->rname);
		if(!cur_xfer->total) {
			tg_text(75, 0, " ???%%");
		} else {
			progr = 100 * cur_xfer->count / cur_xfer->total;
			if(progr < 0) progr = 0;
			if(progr > 100) progr = 100;
			tg_text(75, 0, " %3d%%", progr);
		}
		upd |= 0x8000;
	} else if(prev_xfer) {
		tg_rect(0, 40, 0, 40, 1, 0);
		upd |= 0x8000;
	}
	prev_xfer = cur_xfer;

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

	if(upd) {
		tg_redraw();
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

	case '`':
		tui_invalidate(uilist[0]);
		tui_invalidate(uilist[1]);
		break;

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

	case KB_PGUP:
		if((sel = tui_get_list_sel(uilist[focus]) - 16) < 0) {
			tui_list_sel_start(uilist[focus]);
		} else {
			tui_list_select(uilist[focus], sel);
		}
		break;
	case KB_PGDN:
		if((sel = tui_get_list_sel(uilist[focus]) + 16) >= tui_num_list_items(uilist[focus])) {
			tui_list_sel_end(uilist[focus]);
		} else {
			tui_list_select(uilist[focus], sel);
		}
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

	case KB_F3:
		sel = tui_get_list_sel(uilist[focus]);
		if(focus == 0) {
			struct ftp_transfer *xfer;
			struct ftp_dirent *ent = ftp_dirent(ftp, sel);

			xfer = calloc_nf(1, sizeof *xfer);
			xfer->mem = darr_alloc(0, 1);
			xfer->op = FTP_RETR;
			xfer->rname = strdup_nf(ent->name);
			xfer->total = ent->size;
			xfer->done = view_done;

			cur_xfer = xfer;

			ftp_queue_transfer(ftp, xfer);
		} else {
			/* TODO */
		}
		break;

	case KB_F5:
		sel = tui_get_list_sel(uilist[focus]);
		if(focus == 0) {
			struct ftp_transfer *xfer;
			struct ftp_dirent *ent = ftp_dirent(ftp, sel);
			char *lname = alloca(strlen(ent->name) + 1);

			fixname(lname, ent->name);

			xfer = calloc_nf(1, sizeof *xfer);
			if(!(xfer->fp = fopen(lname, "wb"))) {
				errmsg("failed to open %s: %s\n", lname, strerror(errno));
				free(xfer);
				break;
			}

			xfer->op = FTP_RETR;
			xfer->rname = strdup_nf(ent->name);
			xfer->total = ent->size;
			xfer->done = xfer_done;

			cur_xfer = xfer;

			ftp_queue_transfer(ftp, xfer);
			local_modified = 1;
		} else {
			/* TODO */
		}
		break;

	case KB_F8:
		sel = tui_get_list_sel(uilist[focus]);
		if(focus == 0) {
		} else {
			struct ftp_dirent *ent = localdir + sel;
			if(ent->type == FTP_FILE) {
				infomsg("removing local file: %s\n", ent->name);
				remove(ent->name);
				update_localdir();
				local_modified = 1;
			}
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
	if(!have_lfn) {
		int len;
		char *suffix;
		if((suffix = strrchr(dest, '.'))) {
			*suffix++ = 0;
			if(strlen(suffix) > 3) {
				suffix[3] = 0;
			}
		}
		if((len = strlen(dest)) > 8) {
			dest[8] = 0;
			len = 8;
		}

		if(suffix) {
			dest[len++] = '.';
			if(dest + len != suffix) {
				memmove(dest + len, suffix, strlen(suffix) + 1);
			}
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

	cur_xfer = 0;
}

static void view_done(struct ftp *ftp, struct ftp_transfer *xfer)
{
	view_memopen(xfer->rname, xfer->mem, xfer->count);

	free(xfer->rname);
	free(xfer);

	cur_xfer = 0;
}

static char *clean_line(char *s)
{
	char *end;
	while(*s && isspace(*s)) s++;
	end = s + strlen(s);
	while(end > s && isspace(*--end)) *end = 0;
	return s;
}

int read_servers(const char *fname)
{
	FILE *fp;
	char buf[256];
	char *line, *ptr;
	struct server srv;

	if(!srvlist) {
		srvlist = darr_alloc(0, sizeof *srvlist);
	}

	if(!(fp = fopen(fname, "rb"))) {
		return -1;
	}

	while(fgets(buf, sizeof buf, fp)) {
		line = clean_line(buf);
		if(!*line || *line == '#') continue;

		if(!(ptr = strchr(line, '=')) || !ptr[1]) continue;
		*ptr = 0;

		clean_line(line);
		memset(&srv, 0, sizeof srv);
		srv.name = strdup_nf(line);

		line = clean_line(ptr + 1);

		if(!(ptr = strchr(line, '@'))) {
			srv.host = strdup_nf(line);
		} else {
			*ptr++ = 0;
			if(!*ptr) {
				free(srv.name);
				continue;
			}
			srv.host = strdup_nf(ptr);

			if((ptr = strchr(line, ':')) && ptr > line + 1 && ptr[1]) {
				*ptr = 0;
				srv.user = strdup_nf(line);
				srv.pass = strdup_nf(ptr + 1);
			}
		}

		darr_push(srvlist, &srv);
	}
	return 0;
}

struct server *find_server(const char *name)
{
	int i, num = darr_size(srvlist);

	for(i=0; i<num; i++) {
		if(strcmp(srvlist[i].name, name) == 0) {
			return srvlist + i;
		}
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

#ifdef __DOS__
#include <dos.h>
#include <i86.h>

void detect_lfn(void)
{
	union REGS regs = {0};
	struct SREGS sregs = {0};
	unsigned int drv;
#if __WATCOMC__ < 1200
	unsigned short buf_seg;
#else
	unsigned int buf_seg;
#endif
	char *buf;

	_dos_getdrive(&drv);

	if(_dos_allocmem(3, &buf_seg) != 0) {
		return;
	}
#ifdef _M_I86
#error TODO port to 16bit
#else
	buf = (char*)(buf_seg << 4);
#endif

	sprintf(buf, "%c:\\", 'A' + drv);

	sregs.ds = sregs.es = buf_seg;
	regs.w.ax = 0x71a0;
	regs.w.dx = 0;		/* ds:dx drive letter string */
	regs.w.di = 4;		/* es:di buffer for filesystem name */
	regs.w.cx = 44;		/* buffer size (3*16 - 4) */

	intdosx(&regs, &regs, &sregs);

	if(regs.w.cflag == 0) {
		infomsg("long filenames available\n");
		have_lfn = 1;
	} else {
		infomsg("long filenames not available, will truncate as needed\n");
	}

	_dos_freemem(buf_seg);
}
#endif
