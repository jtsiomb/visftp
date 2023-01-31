#ifndef VIEWER_H_
#define VIEWER_H_

/* takes ownership of the data, and frees it on view_close */
void view_memopen(const char *name, void *start, long size, void (*freefunc)(void*));
void view_close(void);
int view_isopen(void);
void view_update(void);
void view_input(int key);

#endif	/* VIEWER_H_ */
