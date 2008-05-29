## -----------------------------------------------------------------------
##
##   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Main Makefile for SYSLINUX
#

# No builtin rules
MAKEFLAGS += -r
MAKE      += -r

TMPFILE = $(shell mktemp /tmp/gcc_ok.XXXXXX)

CC	 = gcc

gcc_ok   = $(shell tmpf=$(TMPFILE); if $(CC) $(1) dummy.c -o $$tmpf 2>/dev/null; \
	           then echo '$(1)'; else echo '$(2)'; fi; rm -f $$tmpf)

comma   := ,
LDHASH  := $(call gcc_ok,-Wl$(comma)--hash-style=both,)

OSTYPE   = $(shell uname -msr)
INCLUDE  =
CFLAGS   = -W -Wall -Os -fomit-frame-pointer -D_FILE_OFFSET_BITS=64
PIC      = -fPIC
LDFLAGS  = -O2 -s $(LDHASH)
AR	 = ar
RANLIB   = ranlib
LD	 = ld
OBJCOPY  = objcopy
OBJDUMP  = objdump

NASM	 = nasm
NASMOPT  = -O9999
NINCLUDE =
BINDIR   = /usr/bin
SBINDIR  = /sbin
LIBDIR   = /usr/lib
AUXDIR   = $(LIBDIR)/syslinux
MANDIR	 = /usr/man
INCDIR   = /usr/include
TFTPBOOT = /tftpboot

PERL     = perl

VERSION  = $(shell cat version)

%.o: %.c
	$(CC) $(INCLUDE) $(CFLAGS) -c $<

#
# The BTARGET refers to objects that are derived from ldlinux.asm; we
# like to keep those uniform for debugging reasons; however, distributors
# want to recompile the installers (ITARGET).
#
# BOBJECTS and IOBJECTS are the same thing, except used for
# installation, so they include objects that may be in subdirectories
# with their own Makefiles.  Finally, there is a list of those
# directories.
#

# syslinux.exe is BTARGET so as to not require everyone to have the
# mingw suite installed
BTARGET  = version.gen version.h
BOBJECTS = $(BTARGET) \
	mbr/mbr.bin mbr/gptmbr.bin \
	core/pxelinux.0 core/isolinux.bin core/isolinux-debug.bin \
	gpxe/gpxelinux.0 dos/syslinux.com win32/syslinux.exe \
	memdisk/memdisk memdump/memdump.com
# BESUBDIRS and IESUBDIRS are "early", i.e. before the root; BSUBDIRS
# and ISUBDIRS are "late", after the root.
BESUBDIRS = mbr core
BSUBDIRS = memdisk memdump gpxe dos win32
ITARGET  = 
IOBJECTS = $(ITARGET) dos/copybs.com utils/gethostip utils/mkdiskimage \
	mtools/syslinux linux/syslinux extlinux/extlinux
IESUBDIRS =
ISUBDIRS = mtools linux extlinux utils com32 sample

# Things to install in /usr/bin
INSTALL_BIN   =	mtools/syslinux utils/gethostip utils/ppmtolss16 \
		utils/lss16toppm utils/sha1pass utils/md5pass
# Things to install in /sbin
INSTALL_SBIN  = extlinux/extlinux
# Things to install in /usr/lib/syslinux
INSTALL_AUX   =	core/pxelinux.0 gpxe/gpxelinux.0 core/isolinux.bin \
		core/isolinux-debug.bin \
		dos/syslinux.com dos/copybs.com memdisk/memdisk mbr/mbr.bin
INSTALL_AUX_OPT = win32/syslinux.exe

all:
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) ; do $(MAKE) -C $$i $@ ; done
	$(MAKE) all-local
	set -e ; for i in $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done
	-ls -l $(BOBJECTS) $(IOBJECTS)

all-local: $(BTARGET) $(ITARGET)

installer:
	set -e ; for i in $(IESUBDIRS); do $(MAKE) -C $$i all ; done
	$(MAKE) installer-local
	set -e ; for i in $(ISUBDIRS); do $(MAKE) -C $$i all ; done
	-ls -l $(BOBJECTS) $(IOBJECTS)

installer-local: $(ITARGET) $(BINFILES)

version.gen: version version.pl
	$(PERL) version.pl $< $@ '%define'

version.h: version version.pl
	$(PERL) version.pl $< $@ '#define'

install: installer
	mkdir -m 755 -p $(INSTALLROOT)$(BINDIR)
	install -m 755 -c $(INSTALL_BIN) $(INSTALLROOT)$(BINDIR)
	mkdir -m 755 -p $(INSTALLROOT)$(SBINDIR)
	install -m 755 -c $(INSTALL_SBIN) $(INSTALLROOT)$(SBINDIR)
	mkdir -m 755 -p $(INSTALLROOT)$(AUXDIR)
	install -m 644 -c $(INSTALL_AUX) $(INSTALLROOT)$(AUXDIR)
	-install -m 644 -c $(INSTALL_AUX_OPT) $(INSTALLROOT)$(AUXDIR)
	mkdir -m 755 -p $(INSTALLROOT)$(MANDIR)/man1
	install -m 644 -c man/*.1 $(INSTALLROOT)$(MANDIR)/man1
	: mkdir -m 755 -p $(INSTALLROOT)$(MANDIR)/man8
	: install -m 644 -c man/*.8 $(INSTALLROOT)$(MANDIR)/man8
	$(MAKE) -C com32 install

install-lib: installer

install-all: install install-lib

NETINSTALLABLE = pxelinux.0 gpxelinux.0 memdisk/memdisk memdump/memdump.com \
	com32/menu/*.c32 com32/modules/*.c32

netinstall: installer
	mkdir -p $(INSTALLROOT)$(TFTPBOOT)
	install -m 644 $(NETINSTALLABLE) $(INSTALLROOT)$(TFTPBOOT)

local-tidy:
	rm -f *.o *.elf *_bin.c stupid.* patch.offset
	rm -f *.lsr *.lst *.map *.sec
	rm -f $(OBSOLETE)

tidy: local-tidy
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

local-clean:
	rm -f $(ITARGET)

clean: local-tidy local-clean
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

local-dist:
	find . \( -name '*~' -o -name '#*' -o -name core \
		-o -name '.*.d' -o -name .depend \) -type f -print0 \
	| xargs -0rt rm -f

dist: local-dist local-tidy
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

local-spotless:
	rm -f $(BTARGET) .depend *.so.*

spotless: local-clean local-dist local-spotless
	set -e ; for i in $(BESUBDIRS) $(IESUBDIRS) $(BSUBDIRS) $(ISUBDIRS) ; do $(MAKE) -C $$i $@ ; done

local-depend:

depend: local-depend
	$(MAKE) -C memdisk depend

# Shortcut to build linux/syslinux using klibc
klibc:
	$(MAKE) clean
	$(MAKE) CC=klcc ITARGET= ISUBDIRS='linux extlinux' BSUBDIRS=

# Hook to add private Makefile targets for the maintainer.
-include Makefile.private
