#include <errno.h>
#include <sys/select.h>
#include <curses.h>
#include "input.h"

int init_input(void)
{
	return 0;
}

void cleanup_input(void)
{
}

static int convkey(int key)
{
	switch(key) {
	case KEY_DC:
		return KB_DEL;
	case KEY_IC:
		return KB_INS;
	case KEY_UP:
		return KB_UP;
	case KEY_DOWN:
		return KB_DOWN;
	case KEY_LEFT:
		return KB_LEFT;
	case KEY_RIGHT:
		return KB_RIGHT;
	case KEY_HOME:
		return KB_HOME;
	case KEY_END:
		return KB_END;
	case KEY_PPAGE:
		return KB_PGUP;
	case KEY_NPAGE:
		return KB_PGDN;
	default:
		break;
	}
	if(key < 128) {
		return key;
	}
	return -1;
}

int poll_input(union event *ev)
{
	ev->type = EV_KEY;
	if((ev->key.key = convkey(getch())) == -1) {
		return 0;
	}
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
