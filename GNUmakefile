src = $(wildcard src/*.c) $(wildcard src/unix/*.c)
obj = $(src:.c=.o)
dep = $(src:.c=.d)
bin = visftp

warn = -pedantic -Wall -Wno-pointer-sign
dbg = -g
incdir = -Isrc

CFLAGS = $(warn) $(dbg) $(incdir) -MMD
LDFLAGS = -lcurses

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

.PHONY: clean
clean:
	$(RM) $(obj) $(bin)

.PHONY: cleandep
cleandep:
	$(RM) $(dep)
