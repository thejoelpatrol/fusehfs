#!/bin/sh

gcc -o fusehfs -fPIC \
    hfsutils-3.2.6/libhfs/btree.c \
    hfsutils-3.2.6/libhfs/block.c \
    hfsutils-3.2.6/libhfs/data.c \
    hfsutils-3.2.6/libhfs/file.c \
    hfsutils-3.2.6/libhfs/hfs.c \
    hfsutils-3.2.6/libhfs/low.c \
    hfsutils-3.2.6/libhfs/medium.c \
    hfsutils-3.2.6/libhfs/node.c \
    hfsutils-3.2.6/libhfs/record.c \
    hfsutils-3.2.6/libhfs/volume.c \
    hfsutils-3.2.6/libhfs/os/unix.c \
    fusefs_hfs.c log.c main.c \
    -lfuse -D_FILE_OFFSET_BITS=64 -DHAVE_UNISTD_H=1 -DHAVE_FCNTL_H=1 -Ihfsutils-3.2.6/ -Ihfsutils-3.2.6/libhfs -g
