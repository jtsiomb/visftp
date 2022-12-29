#include <conio.h>
#include "tgfx.h"

int main(void)
{
	tg_bgchar(' ');
	tg_clear();

	tg_bgcolor(1);
	tg_rect("Remote", 0, 0, 40, 23, TGFX_FRAME);
	tg_rect("Local", 40, 0, 40, 23, TGFX_FRAME);

	tg_fgcolor(7);
	tg_text(0, 24, "fooolalala bar");
/*	tg_setcursor(2, 24);*/

	while(getch() != 27);

	tg_bgchar(' ');
	tg_bgcolor(0);
	tg_fgcolor(7);
	tg_clear();
	return 0;
}
