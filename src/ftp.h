#ifndef FTP_H_
#define FTP_H_

enum {FTP_DIR, FTP_FILE};	/* ftp_dirent type */

enum {
	FTP_PWD,
	FTP_CHDIR,
	FTP_MKDIR,
	FTP_RMDIR,
	FTP_DEL,
	FTP_LIST,
	FTP_RETR,
	FTP_STORE
};

struct ftp_op {
	int op;
	char *arg;
};

struct ftp_dirent {
	char *name;
	int type;
};

struct ftp {
	int ctl, data;	/* sockets */

	int status;
	char *user, *pass;

	void (*cproc)(struct ftp *ftp, int code, const char *buf, void *cls);
	void (*dproc)(struct ftp *ftp, const char *buf, int sz, void *cls);
	void *cproc_cls, *dproc_cls;

	char crecv[256];
	int num_crecv;
	char drecv[256];
	int num_drecv;

	char *curdir_rem, *curdir_loc;
	struct ftp_dirent *dent;
	int num_dent;

	int modified;
};

struct ftp *ftp_alloc(void);
void ftp_free(struct ftp *ftp);

void ftp_auth(const char *user, const char *pass);

int ftp_connect(struct ftp *ftp, const char *host, int port);
void ftp_close(struct ftp *ftp);

int ftp_sockets(struct ftp *ftp, int *sockv, int maxsize);
int ftp_pending(struct ftp *ftp);

int ftp_handle(struct ftp *ftp, int s);

int ftp_update(struct ftp *ftp);
int ftp_pwd(struct ftp *ftp);
int ftp_chdir(struct ftp *ftp, const char *dirname);


#endif	/* FTP_H_ */
