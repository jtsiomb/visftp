#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include "darray.h"
#include "ftp.h"
#include "util.h"

struct ftp *ftp_alloc(void)
{
	struct ftp *ftp;

	if(!(ftp = malloc(sizeof *ftp))) {
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

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(host->h_addr);
	addr.sin_port = htons(port);

	if(connect(ftp->ctl, (struct sockaddr*)&addr, sizeof addr) == -1) {
		errmsg("failed to connect to: %s (port: %d)\n", hostname, port);
		closesocket(ftp->ctl);
		ftp->ctl = -1;
		return -1;
	}
		
	return 0;
}

void ftp_close(struct ftp *ftp)
{
	if(ftp->ctl >= 0) {
		closesocket(ftp->ctl);
	}
	if(ftp->data >= 0) {
		closesocket(ftp->data);
	}
}

int ftp_update(struct ftp *ftp)
{
	return -1;
}

int ftp_chdir(struct ftp *ftp, const char *dirname)
{
	return -1;
}
