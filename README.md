visftp - visual ftp client with dual-pane text-mode UI
======================================================

![dos (watcom) build status](https://github.com/jtsiomb/visftp/actions/workflows/build_dos_watcom.yml/badge.svg)
![GNU/Linux build status](https://github.com/jtsiomb/visftp/actions/workflows/build_gnulinux.yml/badge.svg)

![visftp shot](http://nuclear.mutantstargoat.com/sw/visftp/img/visftp_shot-thumb.jpg)

visftp is a text-mode graphical ftp client, with a dual-pane UI , also known as
the "orthodox" file management UI paradigm. The dual-pane UI design is a
perfect fit for an FTP client, where the user is required to navigate two
filesystems simultaneously; one pane is dedicated to the remote filesystem
served over ftp, while the other is for navigating the local filesystem.

visftp was written to address the lack of graphical FTP clients for DOS
(MS-DOS, FreeDOS, etc), but will also run on UNIX systems using curses for its
UI.

**Project status**: *Prototype*. visftp mostly works, but it's very rough, and you
will find lots of bugs. Feel free to open bug reports, but keep in mind that the
most obvious issues will be solved as a matter of course as this project
progresses.


License
-------
Copyright (C) 2023 John Tsiombikas <nuclear@mutantstargoat.com>

This program is free software. Feel free to use, modify, and/or redistribute it
under the terms of the GNU General Public License version 3, or any later
version published by the Free Software Foundation. See COPYING for details.

Hotkeys
-------
  - TAB: switch active pane
  - F5: copy selected (upload/download)
  - F7: create new directory
  - F8: delete selected
  - F10: quit
  - `~`: force UI redraw
