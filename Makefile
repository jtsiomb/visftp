obj = main.obj tgfx.obj tui.obj tui_list.obj darray.obj util.obj input.obj &
	ftp.obj viewer.obj treestor.obj ts_text.obj
bin = visftp.exe

!ifdef __UNIX__
incpath = -Isrc -Isrc/dos -Ilibs/watt32
libs = library libs/watt32/wattcpwf.lib
!else
incpath = -Isrc -Isrc\dos -Ilibs\watt32
libs = library libs\watt32\wattcpwf.lib
!endif

#opt = -otexan
warn = -w=3
dbg = -d3

CC = wcc386
LD = wlink
CFLAGS = $(warn) $(dbg) $(opt) $(incpath) $(def) -zq -bt=dos
LDFLAGS = $(libs)

$(bin): $(obj)
	%write objects.lnk $(obj)
	$(LD) debug all option map name $@ system dos4g file { @objects } $(LDFLAGS)

.c: src;src/dos
.asm: src;src/dos

.c.obj: .autodepend
	$(CC) -fo=$@ $(CFLAGS) $<

.asm.obj:
	nasm -f obj -o $@ $[*.asm

!ifdef __UNIX__
clean: .symbolic
	rm -f *.obj
	rm -f objects.lnk
	rm -f visftp.map
	rm -f $(bin)
!else
clean: .symbolic
	del *.obj
	del objects.lnk
	del visftp.map
	del $(bin)
!endif
