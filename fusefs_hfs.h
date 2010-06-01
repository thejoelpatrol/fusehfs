/*
 *  fusefs_hfs.h
 *  FuseHFS
 *
 *  Created by Zydeco on 1/6/2010.
 *  Copyright 2010 namedfork.net. All rights reserved.
 *
 */

struct fusehfs_options {
    char    *path;
    char	*encoding;
	char	*mountpoint;
	int		readonly;
};