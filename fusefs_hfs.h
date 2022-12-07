/*
 *  fusefs_hfs.h
 *  FuseHFS
 *
 *  Created by Zydeco on 1/6/2010.
 *  Copyright 2010 namedfork.net. All rights reserved.
 *
 *  Licensed under GPLv2: https://www.gnu.org/licenses/gpl-2.0.html
 */

struct fusehfs_options {
    char    *path;
    char	*encoding;
	char	*mountpoint;
	int		readonly;
  int partition;
};
