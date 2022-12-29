obj = main.obj tgfx.obj tui.obj darray.obj util.obj input.obj
bin = oftp.exe

!ifdef __UNIX__
incpath = -Isrc -Isrc/dos
!else
incpath = -Isrc -Isrc\dos
!endif

#opt = -otexan
warn = -w=3
dbg = -d3

CC = wcc386
LD = wlink
CFLAGS = $(warn) $(dbg) $(opt) $(incpath) $(def) -zq -bt=dos

$(bin): $(obj)
	%write objects.lnk $(obj)
	$(LD) debug all option map name $@ system dos4g file { @objects } $(LDFLAGS)

.c: src;src/dos
.asm: src;src/dos

.c.obj: .autodepend
	$(CC) -fo=$@ $(CFLAGS) $<

.asm.obj:
	nasm -f obj -o $@ $[*.asm

clean: .symbolic
	del *.obj
	del objects.lnk
	del $(bin)
