#!/bin/sh

# postinstall.sh
# FuseHFS
#
# Created by Zydeco on 28/5/2010.
# Copyright 2010 namedfork.net. All rights reserved.

chown root:wheel /System/Library/Filesystems/fusefs_hfs.fs
ln -fs /System/Library/Filesystems/fusefs_hfs.fs/Contents/Resources/fusefs_hfs /sbin/mount_fusefs_hfs
