#include <errno.h>
#include <sys/select.h>
#include <curses.h>
#include "input.h"

int init_input(void)
{
	nodelay(stdscr, TRUE);
	return 0;
}

void cleanup_input(void)
{
}

int poll_input(union event *ev)
{
	ev->type = EV_KEY;
	ev->key.key = getch();
	return 1;
}

int have_input(void)
{
	fd_set rdset;
	struct timeval tv = {0, 0};

	FD_ZERO(&rdset);
	FD_SET(0, &rdset);

	while(select(1, &rdset, 0, 0, &tv) == -1 && errno == EINTR);

	if(FD_ISSET(0, &rdset)) {
		return 1;
	}
	return 0;
}
