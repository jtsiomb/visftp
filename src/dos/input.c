#include <conio.h>
#include "input.h"

int init_input(void)
{
	return 0;
}

void cleanup_input(void)
{
}

static int conv_special(int key)
{
	switch(key) {
	case 72:
		return KB_UP;
	case 80:
		return KB_DOWN;
	case 75:
		return KB_LEFT;
	case 77:
		return KB_RIGHT;
	case 71:
		return KB_HOME;
	case 79:
		return KB_END;
	case 82:
		return KB_INS;
	case 83:
		return KB_DEL;
	case 73:
		return KB_PGUP;
	case 81:
		return KB_PGDN;
	default:
		break;
	}

	if(key >= 58 && key <= 70) {
		return KB_F1 + (key - 59);
	}

	return -1;
}

int poll_input(union event *ev)
{
	static int special;

	if(have_input()) {
		ev->type = EV_KEY;
		if((ev->key.key = getch()) == 0) {
			special = 1;
			return 0;
		}

		if(special) {
			special = 0;
			if((ev->key.key = conv_special(ev->key.key)) == -1) {
				return 0;
			}
		} else {
			if(ev->key.key == '\r') {
				ev->key.key = '\n';
			}
		}
		return 1;
	}
	return 0;
}

int have_input(void)
{
	return kbhit();
}
