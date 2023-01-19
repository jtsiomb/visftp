#include <conio.h>
#include "input.h"

int init_input(void)
{
	return 0;
}

void cleanup_input(void)
{
}

int poll_input(union event *ev)
{
	if(have_input()) {
		ev->type = EV_KEY;
		ev->key.key = getch();
		return 1;
	}
	return 0;
}

int have_input(void)
{
	return kbhit();
}
