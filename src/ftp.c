#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
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
#endif

#define TIMEOUT	15

static int newconn(struct ftp *ftp);
static int sendcmd(struct ftp *ftp, const char *fmt, ...);
static int handle_control(struct ftp *ftp);
static int handle_data(struct ftp *ftp, int s);
static void proc_control(struct ftp *ftp, const char *buf);

static int cproc_active(struct ftp *ftp, int code, const char *buf, void *cls);
static int cproc_pwd(struct ftp *ftp, int code, const char *buf, void *cls);
static int cproc_cwd(struct ftp *ftp, int code, const char *buf, void *cls);

static int cproc_list(struct ftp *ftp, int code, const char *buf, void *cls);
static void dproc_list(struct ftp *ftp, const char *buf, int sz, void *cls);


struct ftp *ftp_alloc(void)
{
	struct ftp *ftp;

	if(!(ftp = calloc(1, sizeof *ftp))) {
		return 0;
	}
	ftp->ctl = ftp->data = ftp->lis = -1;
	return ftp;
}

void ftp_free(struct ftp *ftp)
{
	if(!ftp) return;

	if(ftp->ctl >= 0) {
		ftp_close(ftp);
	}
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

static void exec_op(struct ftp *ftp, int op, const char *arg)
{
	switch(op) {
	case FTP_PWD:
		ftp_pwd(ftp);
		break;

	case FTP_CHDIR:
		ftp_chdir(ftp, arg);
		break;

	case FTP_MKDIR:
		ftp_mkdir(ftp, arg);
		break;

	case FTP_RMDIR:
		ftp_rmdir(ftp, arg);
		break;

	case FTP_DEL:
		ftp_delete(ftp, arg);
		break;

	case FTP_LIST:
		ftp_list(ftp);
		break;

	case FTP_RETR:
		ftp_retrieve(ftp, arg);
		break;

	case FTP_STORE:
		ftp_store(ftp, arg);
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

	exec_op(ftp, fop->op, fop->arg);

	free(fop->arg);
	free(fop);
}


int ftp_queue(struct ftp *ftp, int op, const char *arg)
{
	struct ftp_op *fop;

	if(!ftp->busy && !ftp->qhead) {
		exec_op(ftp, op, arg);
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

int ftp_waitresp(struct ftp *ftp, time_t timeout)
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
	socklen_t len;
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
	int rd;

	if(ftp->data == -1) {
		return -1;
	}

	for(;;) {
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
	}
}

static int newconn(struct ftp *ftp)
{
	if(ftp_active(ftp) == -1) {
		return -1;
	}
	ftp_queue(ftp, FTP_PWD, 0);
	ftp_queue(ftp, FTP_LIST, 0);
	return 0;
}

int ftp_update(struct ftp *ftp)
{
	return -1;
}

int ftp_pwd(struct ftp *ftp)
{
	sendcmd(ftp, "pwd");
	ftp->cproc = cproc_pwd;
	return 0;
}

int ftp_chdir(struct ftp *ftp, const char *dirname)
{
	sendcmd(ftp, "cwd %s", dirname);
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

struct recvbuf {
	char *buf;
	long size, bufsz;
};

int ftp_list(struct ftp *ftp)
{
	struct recvbuf *rbuf;

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

	free(ftp->curdir_rem);
	ftp->curdir_rem = strdup_nf(dirname);
	ftp->modified = 1;
	return 0;
}

static int cproc_cwd(struct ftp *ftp, int code, const char *buf, void *cls)
{
	return -1;
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

static void free_dirlist(struct ftp_dirent *list)
{
	struct ftp_dirent *tmp;

	while(list) {
		tmp = list;
		list = list->next;

		free(tmp->name);
		free(tmp);
	}
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

static void dproc_list(struct ftp *ftp, const char *buf, int sz, void *cls)
{
	struct recvbuf *rbuf = cls;

	if(sz == 0) {
		/* EOF condition, we got the whole list, update directory entries */
		char *ptr = rbuf->buf;
		char *end = rbuf->buf + rbuf->size;
		struct ftp_dirent *tail = 0;
		struct ftp_dirent *ent;

		free_dirlist(ftp->dent_rem);
		ftp->dent_rem = 0;

		while(ptr < end) {
			ent = malloc_nf(sizeof *ent);
			if(parse_dirent(ent, ptr) != -1) {
				ent->next = 0;
				if(!tail) {
					ftp->dent_rem = tail = ent;
				} else {
					tail->next = ent;
					tail = ent;
				}
			}
			while(ptr < end && *ptr != '\n' && *ptr != '\r') ptr++;
			while(ptr < end && (*ptr == '\r' || *ptr == '\n')) ptr++;
		}
		ftp->modified |= FTP_MOD_REMDIR;

		free(rbuf->buf);
		free(rbuf);
		ftp->dproc = 0;
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
