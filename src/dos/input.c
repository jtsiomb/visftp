#include <conio.h>
#include "input.h"

int init_input(void)
{
	return 0;
}

void cleanup_input(void)
{
}

int wait_input(union event *ev)
{
	ev->type = EV_KEY;
	ev->key.key = getch();
	return 1;
}
