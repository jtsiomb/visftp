#ifndef FTP_H_
#define FTP_H_

enum {FTP_DIR, FTP_FILE};	/* ftp_dirent type */

struct ftp_dirent {
	char *name;
	int type;
};

struct ftp {
	int ctl, data;	/* sockets */

	char crecv[256];
	int num_crecv;
	char drecv[256];
	int num_drecv;

	char *cwd;
	struct ftp_dirent *dent;
	int num_dent;
};

struct ftp *ftp_alloc(void);
void ftp_free(struct ftp *ftp);

int ftp_connect(struct ftp *ftp, const char *host, int port);
void ftp_close(struct ftp *ftp);

int ftp_sockets(struct ftp *ftp, int *sockv, int maxsize);
int ftp_pending(struct ftp *ftp);

int ftp_handle(struct ftp *ftp, int s);

int ftp_update(struct ftp *ftp);
int ftp_chdir(struct ftp *ftp, const char *dirname);


#endif	/* FTP_H_ */
