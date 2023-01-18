#include "tgfx.h"
#include "input.h"
#include "tui.h"
#include "ftp.h"

struct ftp *ftp;
struct tui_widget *uilist;

int main(void)
{
	union event ev;

	if(!(ftp = ftp_alloc())) {
		return 1;
	}
	if(ftp_connect(ftp, "192.168.0.4", 21) == -1) {
		ftp_free(ftp);
		return 1;
	}

	init_input();

	tg_init();

	tg_bgchar(' ');
	tg_clear();

	uilist = tui_list("Remote", 0, 0, 40, 23, 0, 0);
	tui_add_list_item(uilist, "first item");
	tui_add_list_item(uilist, "second item");
	tui_add_list_item(uilist, "another item");
	tui_add_list_item(uilist, "foo");

	tg_setcursor(0, 24);

	tui_draw(uilist);

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
	tg_cleanup();

	cleanup_input();

	ftp_free(ftp);
	return 0;
}
