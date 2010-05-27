# Generated automatically from Makefile.in by configure.
#
# hfsck - tool for checking and repairing the integrity of HFS volumes
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
# $Id: Makefile.in,v 1.6 1998/04/11 23:40:39 rob Exp $
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

COPTS =		-arch i386 -arch ppc
LDOPTS =	

### END OF USER CUSTOMIZATION #################################################

CFLAGS =	$(COPTS) $(INCLUDES) $(DEFINES)
LDFLAGS =	$(LDOPTS)

###############################################################################

TARGETS =	$(HFSCKTARGET)

HFSCKTARGET =	hfsck
HFSCKOBJS =	ck_mdb.o ck_volume.o ck_btree.o hfsck.o util.o main.o  \
			$(LIBOBJS)

###############################################################################

all :: $(TARGETS)

again :: clean all

depend ::
	( sed -n '1,/^### DEPEND/p' Makefile.in;  \
	  echo;  \
	  $(CC) -MM $(INCLUDES) $(DEFINES) *.c;  \
	) > Makefile.in.new
	mv -f Makefile.in.new Makefile.in

clean ::
	rm -f $(TARGETS) *.o gmon.* core

###############################################################################

$(HFSCKTARGET): $(HFSCKOBJS)
	$(CC) $(LDFLAGS) $(HFSCKOBJS) $(LIBS) -o $@

### DEPENDENCIES FOLLOW #######################################################

ck_btree.o: ck_btree.c hfsck.h ../libhfs/libhfs.h ../libhfs/hfs.h \
 ../libhfs/apple.h ../libhfs/data.h ../libhfs/block.h ../libhfs/low.h \
 ../libhfs/file.h ../libhfs/btree.h ../libhfs/node.h \
 ../libhfs/record.h ../libhfs/volume.h util.h ck_btree.h
ck_mdb.o: ck_mdb.c hfsck.h ../libhfs/libhfs.h ../libhfs/hfs.h \
 ../libhfs/apple.h ../libhfs/data.h ../libhfs/block.h ../libhfs/low.h \
 ../libhfs/file.h ../libhfs/btree.h ../libhfs/node.h \
 ../libhfs/record.h ../libhfs/volume.h util.h ck_mdb.h
ck_volume.o: ck_volume.c hfsck.h ../libhfs/libhfs.h ../libhfs/hfs.h \
 ../libhfs/apple.h ../libhfs/data.h ../libhfs/block.h ../libhfs/low.h \
 ../libhfs/file.h ../libhfs/btree.h ../libhfs/node.h \
 ../libhfs/record.h ../libhfs/volume.h ck_volume.h
hfsck.o: hfsck.c hfsck.h ../libhfs/libhfs.h ../libhfs/hfs.h \
 ../libhfs/apple.h ../libhfs/data.h ../libhfs/block.h ../libhfs/low.h \
 ../libhfs/file.h ../libhfs/btree.h ../libhfs/node.h \
 ../libhfs/record.h ../libhfs/volume.h ck_mdb.h ck_volume.h ck_btree.h
main.o: main.c hfsck.h ../libhfs/libhfs.h ../libhfs/hfs.h \
 ../libhfs/apple.h ../libhfs/data.h ../libhfs/block.h ../libhfs/low.h \
 ../libhfs/file.h ../libhfs/btree.h ../libhfs/node.h \
 ../libhfs/record.h ../libhfs/volume.h ../suid.h ../version.h
util.o: util.c hfsck.h ../libhfs/libhfs.h ../libhfs/hfs.h \
 ../libhfs/apple.h ../libhfs/data.h ../libhfs/block.h ../libhfs/low.h \
 ../libhfs/file.h ../libhfs/btree.h ../libhfs/node.h \
 ../libhfs/record.h ../libhfs/volume.h util.h
