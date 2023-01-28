#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/swap.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include "darray.h"
#include "ftp.h"
#include "util.h"

#ifdef __unix__
#include <unistd.h>
#include <fcntl.h>
#define closesocket		close
#define fcntlsocket		fcntl
#else
#define select	select_s

#ifndef O_NONBLOCK
#define O_NONBLOCK	0x2000
#endif
#ifndef F_SETFL
#define F_GETFL	101
#define F_SETFL	102
#endif
#endif

#define TIMEOUT	15

static void free_dir(struct ftp_dirent *dir);

static int newconn(struct ftp *ftp);
static int sendcmd(struct ftp *ftp, const char *fmt, ...);
static int handle_control(struct ftp *ftp);
static int handle_data(struct ftp *ftp, int s);
static void proc_control(struct ftp *ftp, const char *buf);

static int cproc_active(struct ftp *ftp, int code, const char *buf, void *cls);
static int cproc_pasv(struct ftp *ftp, int code, const char *buf, void *cls);
static int cproc_pwd(struct ftp *ftp, int code, const char *buf, void *cls);
static int cproc_cwd(struct ftp *ftp, int code, const char *buf, void *cls);

static int cproc_list(struct ftp *ftp, int code, const char *buf, void *cls);
static void dproc_list(struct ftp *ftp, const char *buf, int sz, void *cls);

static int cproc_xfer(struct ftp *ftp, int code, const char *buf, void *cls);
static void dproc_xfer(struct ftp *ftp, const char *buf, int sz, void *cls);


struct ftp *ftp_alloc(void)
{
	struct ftp *ftp;

	if(!(ftp = calloc(1, sizeof *ftp))) {
		return 0;
	}
	ftp->ctl = ftp->data = ftp->lis = -1;
	ftp->passive = 1;

	ftp->dirent = darr_alloc(0, sizeof *ftp->dirent);
	return ftp;
}

void ftp_free(struct ftp *ftp)
{
	if(!ftp) return;

	if(ftp->ctl >= 0) {
		ftp_close(ftp);
	}

	free_dir(ftp->dirent);
}

static void free_dir(struct ftp_dirent *dir)
{
	int i;
	for(i=0; i<darr_size(dir); i++) {
		free(dir[i].name);
	}
	darr_free(dir);
}

int ftp_connect(struct ftp *ftp, const char *hostname, int port)
{
	struct hostent *host;
	struct sockaddr_in addr;

	if((ftp->ctl = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		errmsg("failed to create control socket\n");
		return -1;
	}

	if(!(host = gethostbyname(hostname))) {
		errmsg("failed to resolve host: %s\n", hostname);
		closesocket(ftp->ctl);
		ftp->ctl = -1;
		return -1;
	}

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr = *((struct in_addr*)host->h_addr);
	addr.sin_port = htons(port);

	if(connect(ftp->ctl, (struct sockaddr*)&addr, sizeof addr) == -1) {
		errmsg("failed to connect to: %s (port: %d)\n", hostname, port);
		closesocket(ftp->ctl);
		ftp->ctl = -1;
		return -1;
	}

	fcntlsocket(ftp->ctl, F_SETFL, fcntlsocket(ftp->ctl, F_GETFL) | O_NONBLOCK);
	ftp->num_crecv = 0;
	return 0;
}

void ftp_close(struct ftp *ftp)
{
	if(ftp->ctl >= 0) {
		closesocket(ftp->ctl);
		ftp->ctl = -1;
		ftp->num_crecv = 0;
	}
	if(ftp->lis >= 0) {
		closesocket(ftp->lis);
		ftp->lis = -1;
	}
	if(ftp->data >= 0) {
		closesocket(ftp->data);
		ftp->data = -1;
	}
}

int ftp_sockets(struct ftp *ftp, int *sockv, int maxsize)
{
	int *sptr = sockv;
	if(ftp->ctl >= 0 && maxsize-- > 0) {
		*sptr++ = ftp->ctl;
	}
	if(ftp->lis >= 0 && maxsize-- > 0) {
		*sptr++ = ftp->lis;
	}
	if(ftp->data >= 0 && maxsize-- > 0) {
		*sptr++ = ftp->data;
	}
	return sptr - sockv;
}

static void exec_op(struct ftp *ftp, struct ftp_op *fop)
{
	switch(fop->op) {
	case FTP_TYPE:
		ftp_type(ftp, fop->arg);
		break;

	case FTP_PWD:
		ftp_pwd(ftp);
		break;

	case FTP_CHDIR:
		ftp_chdir(ftp, fop->arg);
		break;

	case FTP_CDUP:
		ftp_chdir(ftp, "..");
		break;

	case FTP_MKDIR:
		ftp_mkdir(ftp, fop->arg);
		break;

	case FTP_RMDIR:
		ftp_rmdir(ftp, fop->arg);
		break;

	case FTP_DEL:
		ftp_delete(ftp, fop->arg);
		break;

	case FTP_LIST:
		ftp_list(ftp);
		break;

	case FTP_RETR:
		ftp_retrieve(ftp, fop->arg);
		break;

	case FTP_STORE:
		ftp_store(ftp, fop->arg);
		break;

	case FTP_XFER:
		ftp_transfer(ftp, fop->data);
		break;

	default:
		break;
	}
}

static void exec_queued(struct ftp *ftp)
{
	struct ftp_op *fop;

	if(!(fop = ftp->qhead)) {
		return;
	}

	if(ftp->qtail == fop) {
		ftp->qhead = ftp->qtail = 0;
	} else {
		ftp->qhead = ftp->qhead->next;
	}

	exec_op(ftp, fop);

	free(fop->arg);
	free(fop);
}


int ftp_queue(struct ftp *ftp, int op, const char *arg)
{
	struct ftp_op *fop;

	if(!ftp->busy && !ftp->qhead) {
		struct ftp_op tmp = {0};
		tmp.op = op;
		tmp.arg = (char*)arg;
		exec_op(ftp, &tmp);
		return 0;
	}

	if(!(fop = malloc(sizeof *fop))) {
		return -1;
	}
	if(arg) {
		if(!(fop->arg = strdup(arg))) {
			free(fop);
			return -1;
		}
	} else {
		fop->arg = 0;
	}
	fop->op = op;
	fop->next = 0;

	if(ftp->qhead) {
		ftp->qtail->next = fop;
		ftp->qtail = fop;
	} else {
		ftp->qhead = ftp->qtail = fop;
	}
	return 0;
}

int ftp_queue_transfer(struct ftp *ftp, struct ftp_transfer *xfer)
{
	struct ftp_op *fop;

	if(!ftp->busy && !ftp->qhead) {
		struct ftp_op tmp = {0};
		tmp.op = FTP_XFER;
		tmp.data = xfer;
		exec_op(ftp, &tmp);
		return 0;
	}

	if(!(fop = malloc(sizeof *fop))) {
		return -1;
	}
	fop->op = FTP_XFER;
	fop->arg = 0;
	fop->data = xfer;
	fop->next = 0;

	if(ftp->qhead) {
		ftp->qtail->next = fop;
		ftp->qtail = fop;
	} else {
		ftp->qhead = ftp->qtail = fop;
	}
	return 0;
}

int ftp_waitresp(struct ftp *ftp, long timeout)
{
	fd_set rdset;
	struct timeval tv;
	time_t start;

	ftp->last_resp = -1;
	start = time(0);

	for(;;) {
		FD_ZERO(&rdset);
		FD_SET(ftp->ctl, &rdset);

		if(timeout >= 0) {
			tv.tv_sec = timeout;
			tv.tv_usec = 0;
		}

		if(select(ftp->ctl + 1, &rdset, 0, 0, timeout >= 0 ? &tv : 0) == -1 && errno == EINTR) {
			continue;
		}

		if(FD_ISSET(ftp->ctl, &rdset)) {
			ftp->last_resp = -1;
			ftp_handle(ftp, ftp->ctl);
			if(ftp->last_resp) {
				break;
			}
		}

		if(timeout > 0) {
			timeout -= time(0) - start;
			if(timeout <= 0) {
				return -1;
			}
		}

	}
	return ftp->last_resp;
}

static int ftp_active(struct ftp *ftp)
{
	struct sockaddr_in sa = {0};
	int len;
	unsigned long addr;
	unsigned short port;

	if(ftp->lis >= 0) {
		closesocket(ftp->lis);
	}
	if(ftp->data >= 0) {
		closesocket(ftp->data);
		ftp->data = -1;
	}

	if((ftp->lis = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		errmsg("ftp_active: failed to create listening socket\n");
		return -1;
	}

	len = sizeof sa;
	getsockname(ftp->ctl, (struct sockaddr*)&sa, &len);

	sa.sin_family = AF_INET;
	sa.sin_port = 0;

	if(bind(ftp->lis, (struct sockaddr*)&sa, sizeof sa) == -1) {
		errmsg("ftp_active: failed to bind listening socket\n");
		closesocket(ftp->lis);
		ftp->lis = -1;
		return -1;
	}
	listen(ftp->lis, 1);

	len = sizeof sa;
	if(getsockname(ftp->lis, (struct sockaddr*)&sa, &len) == -1) {
		errmsg("ftp_active: failed to retrieve listening socket address\n");
		closesocket(ftp->lis);
		ftp->lis = -1;
		return -1;
	}

	addr = ntohl(sa.sin_addr.s_addr);
	port = ntohs(sa.sin_port);
	infomsg("listening on %s port %d\n", inet_ntoa(sa.sin_addr), port);

	sendcmd(ftp, "PORT %d,%d,%d,%d,%d,%d", addr >> 24, (addr >> 16) & 0xff,
			(addr >> 8) & 0xff, addr & 0xff, port >> 8, port & 0xff);

	ftp->cproc = cproc_active;
	return 0;
}

static int ftp_passive(struct ftp *ftp)
{
	if(ftp->lis >= 0) {
		closesocket(ftp->lis);
		ftp->lis = -1;
	}
	if(ftp->data >= 0) {
		closesocket(ftp->data);
		ftp->data = -1;
	}

	sendcmd(ftp, "PASV");
	ftp->cproc = cproc_pasv;
	return 0;
}

int ftp_handle(struct ftp *ftp, int s)
{
	if(s == ftp->ctl) {
		return handle_control(ftp);
	}
	if(s == ftp->data) {
		return handle_data(ftp, s);
	}
	if(s == ftp->lis) {
		int ns = accept(s, 0, 0);
		if(ftp->data >= 0) {
			closesocket(ns);
		} else {
			ftp->data = ns;
		}
		return 0;
	}
	return -1;
}

static int sendcmd(struct ftp *ftp, const char *fmt, ...)
{
	char buf[256];
	va_list ap;

	if(ftp->ctl < 0) {
		return -1;
	}

	ftp->busy = 1;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	infomsg("send: %s\n", buf);
	strcat(buf, "\r\n");
	return send(ftp->ctl, buf, strlen(buf), 0);
}

static int handle_control(struct ftp *ftp)
{
	int i, sz, rd;
	char *buf, *start, *end;

	for(;;) {
		if((sz = sizeof ftp->crecv - ftp->num_crecv) <= 0) {
			/* discard buffer */
			warnmsg("discard buffer\n");
			sz = sizeof ftp->crecv;
			ftp->num_crecv = 0;
		}
		start = ftp->crecv + ftp->num_crecv;
		if((rd = recv(ftp->ctl, start, sz, 0)) == -1) {
			if(errno == EINTR) continue;
			/* assume EWOULDBLOCK, try again next time */
			return 0;
		}
		if(rd == 0) {
			ftp_close(ftp);
			return -1;
		}

		end = start + rd;
		buf = ftp->crecv;
		for(i=0; i<rd; i++) {
			if(start[i] == '\r') {
				start[i] = 0;
			} else if(start[i] == '\n') {
				start[i] = 0;
				proc_control(ftp, buf);
				buf = start + i + 1;
			}
		}
		if(buf != ftp->crecv && buf < end) {
			ftp->num_crecv = end - buf;
			memmove(ftp->crecv, buf, ftp->num_crecv);
		} else {
			ftp->num_crecv = 0;
		}
	}
	return 0;
}

static int handle_data(struct ftp *ftp, int s)
{
	int i, rd;

	if(ftp->data == -1) {
		return -1;
	}

	/* get at most 4 packets at a time, to allow returning back to the main loop
	 * to process input
	 */
	for(i=0; i<4; i++) {
		if((rd = recv(ftp->data, ftp->drecv, sizeof ftp->drecv, 0)) == -1) {
			if(errno == EINTR) continue;
			/* assume EWOULDBLOCK, try again next time */
			break;
		}

		/* call the callback first, so that we'll get a 0-count call to indicate
		 * EOF when the server closes the data connection
		 */
		if(ftp->dproc) {
			ftp->dproc(ftp, ftp->drecv, rd, ftp->dproc_cls);
		}

		if(rd == 0) {
			closesocket(ftp->data);
			ftp->data = -1;
			return -1;
		}
	}
	return 0;
}

static int respcode(const char *resp)
{
	if(!isdigit(resp[0]) || !isdigit(resp[1]) || !isdigit(resp[2])) {
		return 0;
	}

	if(isspace(resp[3])) {
		return atoi(resp);
	}
	if(resp[3] == '-') {
		return -atoi(resp);
	}
	return 0;
}

static void proc_control(struct ftp *ftp, const char *buf)
{
	int code;
	char *end;

	while(*buf && isspace(*buf)) buf++;
	if((end = strchr(buf, '\r'))) {
		*end = 0;
	}

	infomsg("recv: %s\n", buf);

	if((code = respcode(buf)) == 0) {
		warnmsg("ignoring invalid response: %s\n", buf);
		return;
	}
	if(code < 0) {
		return;	/* ignore continuations for now */
	}

	ftp->last_resp = code;

	if(ftp->cproc) {
		if(ftp->cproc(ftp, code, buf, ftp->cproc_cls) <= 0) {
			ftp->cproc = 0;
			ftp->busy = 0;

			/* execute next operation if there's one queued */
			exec_queued(ftp);
		}
		return;
	}
	ftp->busy = 0;

	switch(code) {
	case 220:
		sendcmd(ftp, "user %s", ftp->user ? ftp->user : "anonymous");
		break;
	case 331:
		sendcmd(ftp, "pass %s", ftp->pass ? ftp->pass : "foobar");
		break;
	case 230:
		infomsg("login successful\n");
		if(newconn(ftp) == -1) {
			ftp_close(ftp);
		}
		break;
	case 530:
		ftp->status = 0;
		errmsg("login failed\n");
		break;

	default:
		ftp->busy = 0;

		/* execute next operation if there's one queued */
		exec_queued(ftp);
	}
}

static int newconn(struct ftp *ftp)
{
	ftp_queue(ftp, FTP_TYPE, "I");
	ftp_queue(ftp, FTP_PWD, 0);
	ftp_queue(ftp, FTP_LIST, 0);
	return 0;
}

int ftp_update(struct ftp *ftp)
{
	return -1;
}

int ftp_type(struct ftp *ftp, const char *type)
{
	int tc = toupper(type[0]);
	if(tc != 'I' && tc != 'A') {
		return -1;
	}
	sendcmd(ftp, "type %c", tc);
	return 0;
}

int ftp_pwd(struct ftp *ftp)
{
	sendcmd(ftp, "pwd");
	ftp->cproc = cproc_pwd;
	return 0;
}

int ftp_chdir(struct ftp *ftp, const char *dirname)
{
	if(strcmp(dirname, "..") == 0) {
		sendcmd(ftp, "cdup");
	} else {
		sendcmd(ftp, "cwd %s", dirname);
	}
	ftp->cproc = cproc_cwd;
	return 0;
}

int ftp_mkdir(struct ftp *ftp, const char *dirname)
{
	return -1;
}

int ftp_rmdir(struct ftp *ftp, const char *dirname)
{
	return -1;
}

int ftp_delete(struct ftp *ftp, const char *fname)
{
	return -1;
}

static int prepare_data(struct ftp *ftp)
{
	if(ftp->data >= 0) {
		return 0;
	}

	if(ftp->passive) {
		ftp_passive(ftp);
		ftp_waitresp(ftp, TIMEOUT);
	} else {
		if(ftp_active(ftp) == -1) {
			return -1;
		}
	}
	if(ftp->data == -1) {
		return -1;
	}
	return 0;
}

struct recvbuf {
	char *buf;
	long size, bufsz;
};

int ftp_list(struct ftp *ftp)
{
	struct recvbuf *rbuf;

	if(prepare_data(ftp) == -1) {
		return -1;
	}

	if(!(rbuf = malloc(sizeof *rbuf))) {
		errmsg("failed to allocate receive buffer\n");
		return -1;
	}
	rbuf->size = 0;
	rbuf->bufsz = 1024;
	if(!(rbuf->buf = malloc(rbuf->bufsz))) {
		free(rbuf);
		errmsg("failed to allocate receive buffer\n");
		return -1;
	}

	sendcmd(ftp, "list");
	ftp->cproc = cproc_list;
	ftp->dproc = dproc_list;
	ftp->cproc_cls = ftp->dproc_cls = rbuf;
	return 0;
}

int ftp_retrieve(struct ftp *ftp, const char *fname)
{
	return -1;
}

int ftp_store(struct ftp *ftp, const char *fname)
{
	return -1;
}

#define XFER_BUF_SIZE	1024

int ftp_transfer(struct ftp *ftp, struct ftp_transfer *xfer)
{
	if(prepare_data(ftp) == -1) {
		return -1;
	}

	if(xfer->op == FTP_RETR) {
		sendcmd(ftp, "retr %s", xfer->rname);
		ftp->cproc = cproc_xfer;
		ftp->dproc = dproc_xfer;
		ftp->cproc_cls = ftp->dproc_cls = xfer;
	} else {
		/* TODO */
	}
	return 0;
}

static int get_quoted_text(const char *str, char *buf)
{
	int len;
	const char *src, *end;

	if(!(src = strchr(str, '"'))) {
		return -1;
	}
	src++;
	end = src;
	while(*end && *end != '"') end++;
	if(!*end) return -1;

	len = end - src;
	memcpy(buf, src, len);
	buf[len] = 0;
	return 0;
}

static int cproc_active(struct ftp *ftp, int code, const char *buf, void *cls)
{
	if(code != 200) {
		errmsg("ftp_active failed\n");
		ftp_close(ftp);
	} else {
		ftp->status = FTP_CONN_ACT;
	}
	return 0;
}

static int cproc_pasv(struct ftp *ftp, int code, const char *buf, void *cls)
{
	const char *str;
	unsigned int addr[6];
	struct sockaddr_in sa;
	unsigned int ipaddr;
	int port;

	if(code != 227) {
		errmsg("ftp_passive failed\n");
		goto nopasv;
	}

	str = buf;
	while(*str) {
		if(sscanf(str, "%u,%u,%u,%u,%u,%u", addr, addr + 1, addr + 2, addr + 3,
					addr + 4, addr + 5) == 6) {
			break;
		}
		str++;
	}
	if(!*str || (addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]) >= 256) {
		errmsg("ftp_passive: failed to parse response: %s\n", buf);
		goto nopasv;
	}
	port = (addr[4] << 8) | addr[5];
	ipaddr = ((unsigned int)addr[0] << 24) | ((unsigned int)addr[1] << 16) |
		((unsigned int)addr[2] << 8) | addr[3];

	if((ftp->data = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "ftp_passive: failed to allocate socket\n");
		goto nopasv;
	}

	infomsg("passive mode: %d.%d.%d.%d port %d\n", addr[0], addr[1], addr[2], addr[3],
			port);

	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(ipaddr);
	sa.sin_port = htons(port);

	if(connect(ftp->data, (struct sockaddr*)&sa, sizeof sa) == -1) {
		errmsg("ftp_passive: failed to connect to %s:%p\n", inet_ntoa(sa.sin_addr), port);
		closesocket(ftp->data);
		ftp->data = -1;
		goto nopasv;
	}

	ftp->status = FTP_CONN_PASV;
	return 0;

nopasv:
	ftp->passive = 0;
	ftp_active(ftp);
	return 0;
}

static int cproc_pwd(struct ftp *ftp, int code, const char *buf, void *cls)
{
	char *dirname;

	if(code != 257) {
		warnmsg("pwd failed\n");
		return -1;
	}

	dirname = alloca(strlen(buf) + 1);
	if(get_quoted_text(buf, dirname) == -1) {
		warnmsg("pwd: invalid response: %s\n", buf);
		return -1;
	}

	free(ftp->curdir);
	ftp->curdir = strdup_nf(dirname);
	ftp->modified = FTP_MOD_DIR;
	return 0;
}

static int cproc_cwd(struct ftp *ftp, int code, const char *buf, void *cls)
{
	if(code != 250) {
		warnmsg("cwd failed\n");
		return -1;
	}

	ftp_queue(ftp, FTP_PWD, 0);
	ftp_queue(ftp, FTP_LIST, 0);
	return 0;
}

static int cproc_list(struct ftp *ftp, int code, const char *buf, void *cls)
{
	if(code < 200) {
		/* expect more */
		return 1;
	}

	if(code >= 400) {
		errmsg("failed to retrieve directory listing\n");
	}
	return 0;
}

#define SKIP_FIELD(p) \
	do { \
		while(*(p) && *(p) != '\n' && !isspace(*(p))) (p)++; \
		while(*(p) && *(p) != '\n' && isspace(*(p))) (p)++; \
	} while(0)

static int parse_dirent(struct ftp_dirent *ent, const char *line)
{
	int len;
	const char *ptr = line;
	const char *end;

	if(!(end = strchr(line, '\r')) && !(end = strchr(line, '\n'))) {
		return -1;
	}

	if(line[0] == 'd') {
		ent->type = FTP_DIR;
	} else {
		ent->type = FTP_FILE;
	}

	SKIP_FIELD(ptr);		/* skip mode */
	SKIP_FIELD(ptr);		/* skip links */
	SKIP_FIELD(ptr);		/* skip owner */
	SKIP_FIELD(ptr);		/* skip group */

	if(ent->type == FTP_FILE) {
		ent->size = atoi(ptr);
	}
	SKIP_FIELD(ptr);		/* skip size */
	SKIP_FIELD(ptr);		/* skip month */
	SKIP_FIELD(ptr);		/* skip day */
	SKIP_FIELD(ptr);		/* skip year */

	if(ptr >= end) return -1;

	len = end - ptr;
	ent->name = malloc(len + 1);
	memcpy(ent->name, ptr, len);
	ent->name[len] = 0;

	return 0;
}

int ftp_direntcmp(const void *a, const void *b)
{
	const struct ftp_dirent *da = a, *db = b;

	if(da->type == db->type) {
		return strcmp(da->name, db->name);
	}
	return da->type == FTP_DIR ? -1 : 1;
}

static void dproc_list(struct ftp *ftp, const char *buf, int sz, void *cls)
{
	int num;
	struct recvbuf *rbuf = cls;

	if(sz == 0) {
		/* EOF condition, we got the whole list, update directory entries */
		char *ptr = rbuf->buf;
		char *end = rbuf->buf + rbuf->size;
		struct ftp_dirent ent;

		darr_clear(ftp->dirent);

		while(ptr < end) {
			if(parse_dirent(&ent, ptr) != -1) {
				darr_push(ftp->dirent, &ent);
			}
			while(ptr < end && *ptr != '\n' && *ptr != '\r') ptr++;
			while(ptr < end && (*ptr == '\r' || *ptr == '\n')) ptr++;
		}
		ftp->modified |= FTP_MOD_DIR;

		free(rbuf->buf);
		free(rbuf);
		ftp->dproc = 0;

		num = darr_size(ftp->dirent);
		qsort(ftp->dirent, num, sizeof *ftp->dirent, ftp_direntcmp);
		return;
	}

	if(rbuf->size + sz > rbuf->bufsz) {
		char *tmp;
		int newsz = rbuf->bufsz << 1;

		if(!(tmp = realloc(rbuf->buf, newsz))) {
			errmsg("failed to resize receive buffer\n");
			return;
		}
		rbuf->buf = tmp;
		rbuf->bufsz = newsz;
	}

	memcpy(rbuf->buf + rbuf->size, buf, sz);
	rbuf->size += sz;
}

static int cproc_xfer(struct ftp *ftp, int code, const char *buf, void *cls)
{
	char *ptr;
	struct ftp_transfer *xfer = cls;

	if(code < 200) {
		/* expect more */
		if(code == 150 && (ptr = strchr(buf, '('))) {
			/* update total size */
			sscanf(ptr, "(%ld bytes)", &xfer->total);
		}
		return 1;
	}

	if(code >= 400) {
		errmsg("failed to retrieve file\n");
	}
	return 0;
}

static void dproc_xfer(struct ftp *ftp, const char *buf, int sz, void *cls)
{
	struct ftp_transfer *xfer = cls;

	if(xfer->op == FTP_RETR) {
		if(sz == 0) {
			/* EOF condition, got the whole file */
			if(xfer->fp) {
				fclose(xfer->fp);
				xfer->fp = 0;
			}

			if(xfer->done) {
				xfer->done(ftp, xfer);
			}
			return;
		}

		if(xfer->fp) {
			((char*)buf)[sz] = 0;
			fwrite(buf, 1, sz, xfer->fp);

		} else if(xfer->mem) {
			int prevsz = darr_size(xfer->mem);
			darr_resize(xfer->mem, prevsz + sz);
			memcpy(xfer->mem + prevsz, buf, sz);
		}

		xfer->count += sz;
		if(xfer->count > xfer->total) {
			xfer->count = xfer->total;
		}

	} else { /* FTP_STOR */
		/* TODO */
	}
}

const char *ftp_curdir(struct ftp *ftp)
{
	return ftp->curdir;
}

int ftp_num_dirent(struct ftp *ftp)
{
	return darr_size(ftp->dirent);
}

struct ftp_dirent *ftp_dirent(struct ftp *ftp, int idx)
{
	return ftp->dirent + idx;
}
