# Generated automatically from Makefile.in by configure.
#
# hfsutils - tools for reading and writing Macintosh HFS volumes
# Copyright (C) 1996-1998 Robert Leslie
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Id: Makefile.in,v 1.4 1998/04/11 23:40:08 rob Exp $
#


src =		.

### USER CUSTOMIZATIONS FOLLOW ################################################

prefix =	/usr/local
exec_prefix =	${prefix}

MANDEST =	${prefix}/man

INCDEST =	${prefix}/include
LIBDEST =	${exec_prefix}/lib

INSTALL =	/usr/bin/install -c
LIBINSTALL =	${INSTALL} -m 644

CC =		gcc
INCLUDES =	 -I.. -I../libhfs
DEFINES =	-DHAVE_CONFIG_H
LIBOBJS =	 ../suid.o ../version.o
LIBS =		 -L../libhfs -lhfs

COPTS =		-g -O2
LDOPTS =	

### END OF USER CUSTOMIZATION #################################################

CFLAGS =	$(COPTS) $(INCLUDES) $(DEFINES)
LDFLAGS =	$(LDOPTS)

###############################################################################

TARGETS =	$(MKFSTARGET) $(FSCKTARGET) $(HDUMPTARGET)

MKFSTARGET =	mkfs.hfs
FSCKTARGET =	fsck.hfs
HDUMPTARGET =	hdump

SUBDIRS =	kernel

MKFSOBJS =	mkfs.o $(LIBOBJS)
FSCKOBJS =	fsck.o $(LIBOBJS)
HDUMPOBJS =	hdump.o $(LIBOBJS)

###############################################################################

all :: $(TARGETS)

again :: clean all

depend ::
	for dir in $(SUBDIRS); do  \
		(cd $$dir && $(MAKE) depend);  \
	done

	( sed -n '1,/^### DEPEND/p' Makefile.in;  \
	  echo;  \
	  $(CC) -MM $(INCLUDES) $(DEFINES) *.c;  \
	) > Makefile.in.new
	mv -f Makefile.in.new Makefile.in

clean ::
	for dir in $(SUBDIRS); do  \
		(cd $$dir && $(MAKE) clean);  \
	done

	rm -f $(TARGETS) *.o gmon.* core

###############################################################################

$(MKFSTARGET): $(MKFSOBJS)
	$(CC) $(LDFLAGS) $(MKFSOBJS) $(LIBS) -o $@

$(FSCKTARGET): $(FSCKOBJS)
	$(CC) $(LDFLAGS) $(FSCKOBJS) $(LIBS) -o $@

$(HDUMPTARGET): $(HDUMPOBJS)
	$(CC) $(LDFLAGS) $(HDUMPOBJS) $(LIBS) -o $@

### DEPENDENCIES FOLLOW #######################################################

fsck.o: fsck.c
hdump.o: hdump.c ../libhfs/libhfs.h ../libhfs/hfs.h ../libhfs/apple.h \
 ../libhfs/volume.h ../libhfs/low.h ../libhfs/data.h
mkfs.o: mkfs.c
