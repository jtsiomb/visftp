#include "tgfx.h"
#include "input.h"

int main(void)
{
	union event ev;

	init_input();

	tg_bgchar(' ');
	tg_clear();

	tg_bgcolor(1);
	tg_rect("Remote", 0, 0, 40, 23, TGFX_FRAME);
	tg_rect("Local", 40, 0, 40, 23, TGFX_FRAME);

	tg_bgcolor(0);
	tg_fgcolor(7);
	tg_text(0, 23, ">");
	tg_setcursor(2, 23);

	while(wait_input(&ev)) {
		switch(ev.type) {
		case EV_KEY:
			if(ev.key.key == 27) goto done;
			break;

		default:
			break;
		}
	}

done:
	tg_bgchar(' ');
	tg_bgcolor(0);
	tg_fgcolor(7);
	tg_clear();

	cleanup_input();
	return 0;
}
