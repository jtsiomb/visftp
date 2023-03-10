#ifndef INPUT_H_
#define INPUT_H_

enum {
	KB_DEL = 128,
	KB_INS,
	KB_UP,
	KB_DOWN,
	KB_LEFT,
	KB_RIGHT,
	KB_HOME,
	KB_END,
	KB_PGUP,
	KB_PGDN,

	KB_F1,
	KB_F2,
	KB_F3,
	KB_F4,
	KB_F5,
	KB_F6,
	KB_F7,
	KB_F8,
	KB_F9,
	KB_F10,
	KB_F11,
	KB_F12
};

enum {
	EV_KEY,
	EV_MMOVE,
	EV_MBUTTON
};

struct event_key {
	int type;
	int key;
};

struct event_mmove {
	int type;
	int x, y;
};

struct event_mbutton {
	int type;
	int x, y, bn, press;
};

union event {
	int type;
	struct event_key key;
	struct event_mmove mmove;
	struct event_mbutton mbutton;
};


int init_input(void);
void cleanup_input(void);

int poll_input(union event *ev);
int have_input(void);

#endif	/* INPUT_H_ */
