#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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

static int handle_control(struct ftp *ftp);
static int handle_data(struct ftp *ftp, int s);
static void proc_control(struct ftp *ftp, const char *buf);

struct ftp *ftp_alloc(void)
{
	struct ftp *ftp;

	if(!(ftp = calloc(1, sizeof *ftp))) {
		return 0;
	}
	ftp->ctl = ftp->data = -1;
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
	if(ftp->data >= 0) {
		closesocket(ftp->data);
		ftp->data = -1;
		ftp->num_drecv = 0;
	}
}

int ftp_sockets(struct ftp *ftp, int *sockv, int maxsize)
{
	if(ftp->ctl >= 0 && maxsize) {
		*sockv++ = ftp->ctl;
		maxsize--;
	}
	return 1;
}

int ftp_pending(struct ftp *ftp)
{
	int maxfd = -1;
	fd_set rdset;
	struct timeval tv = {0, 0};

	FD_ZERO(&rdset);
	if(ftp->ctl >= 0) {
		FD_SET(ftp->ctl, &rdset);
		maxfd = ftp->ctl;
	}
	if(ftp->data >= 0) {
		FD_SET(ftp->data, &rdset);
		if(ftp->data > maxfd) maxfd = ftp->data;
	}
	if(maxfd == -1) return 0;

	return select(maxfd + 1, &rdset, 0, 0, &tv) > 0 ? 1 : 0;
}

int ftp_handle(struct ftp *ftp, int s)
{
	if(s == ftp->ctl) {
		return handle_control(ftp);
	}
	if(s == ftp->data) {
		return handle_data(ftp, s);
	}
	return -1;
}

static int handle_control(struct ftp *ftp)
{
	int i, sz, rd;
	char *buf, *start, *end;

	while((sz = sizeof ftp->crecv - ftp->num_crecv) > 0) {
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
			if(start[i] == '\n') {
				start[i] = 0;
				proc_control(ftp, buf);
				buf = start + i + 1;
			}
		}
		if(buf != ftp->crecv && buf < end) {
			ftp->num_crecv = end - buf;
			memmove(ftp->crecv, buf, ftp->num_crecv);
		}
	}
	return 0;
}

static int handle_data(struct ftp *ftp, int s)
{
	return -1;
}

static void proc_control(struct ftp *ftp, const char *buf)
{
}

int ftp_update(struct ftp *ftp)
{
	return -1;
}

int ftp_chdir(struct ftp *ftp, const char *dirname)
{
	return -1;
}
