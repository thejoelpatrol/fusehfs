/*
 * main.c
 * FuseHFS
 *
 * Created by Zydeco on 27/2/2010.
 * Copyright 2010 namedfork.net. All rights reserved.
 *
 */

#include "fuse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iconv.h>
#include <libhfs/hfs.h>

#define FUSEHFS_VERSION "0.1"

extern struct fuse_operations FuseHFS_operations;
extern iconv_t iconv_to_utf8, iconv_to_mac;

static struct {
    char    *path;
    char	*encoding;
	char	*mountpoint;
	int		readonly;
} options = {
    .path =         NULL,
    .encoding =		NULL,
	.readonly =		0
};

char * hfs_to_utf8 (const char * in, char * out, size_t outlen);
char * utf8_to_hfs (const char * in);

enum {
	KEY_VERSION,
	KEY_HELP,
	KEY_ENCODING,
	KEY_READONLY,
};

static struct fuse_opt FuseHFS_opts[] = {
	FUSE_OPT_KEY("-V",			KEY_VERSION),
	FUSE_OPT_KEY("--version",	KEY_VERSION),
	FUSE_OPT_KEY("-h",			KEY_HELP),
	FUSE_OPT_KEY("--help",		KEY_HELP),
	FUSE_OPT_KEY("--encoding=",	KEY_ENCODING),
	FUSE_OPT_KEY("--readonly",	KEY_READONLY),
	FUSE_OPT_END
};

static int FuseHFS_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
	switch (key) {
		case FUSE_OPT_KEY_NONOPT:
			if (options.path == NULL) {
				options.path = strdup(arg);
				return 0;
			}
			return 1;
		case KEY_ENCODING:
			if (options.encoding == NULL) {
				options.encoding = strdup(arg+11);
				return 0;
			}
			return 1;
		case KEY_VERSION:
			printf("FuseHFS %s, (c)2010 namedfork.net namedfork.net\n", FUSEHFS_VERSION);
			exit(1);
		case KEY_HELP:
			printf("usage: fusefs_hfs [fuse options] device mountpoint\n");
			exit(0);
		case KEY_READONLY:
			options.readonly = 1;
			return 0;
	}
	return 0;
}

int main(int argc, char* argv[], char* envp[], char** exec_path) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	
	bzero(&options, sizeof options);
	if (fuse_opt_parse(&args, NULL, FuseHFS_opts, FuseHFS_opt_proc)) return 1;
	if (options.encoding == NULL) options.encoding = strdup("Macintosh");
	
	// create iconv
	iconv_to_utf8 = iconv_open("UTF-8", options.encoding);
	if (iconv_to_utf8 == (iconv_t)-1) {
		perror("iconv_open");
		exit(1);
	}
	iconv_to_mac = iconv_open(options.encoding, "UTF-8");
	if (iconv_to_mac == (iconv_t)-1) {
		perror("iconv_open");
		exit(1);
	}
	
	// mount volume
	hfsvolent vstat;
	int mode = options.readonly?HFS_MODE_RDONLY:HFS_MODE_ANY;
	if (NULL == hfs_mount(options.path, 0, mode)) {
		perror("hfs_mount");
		return 1;
	}
	if (-1 == hfs_vstat(NULL, &vstat)) {
		perror("hfs_vstat");
		return 1;
	}
	atexit(hfs_umountall);
	
	// is it read-only?
	if (vstat.flags & HFS_ISLOCKED) {
		printf("Mounting read only\n");
		fuse_opt_add_arg(&args, "-oro");
	}
	fuse_opt_add_arg(&args, "-s");

	// MacFUSE options
#if defined(__APPLE__)
    char volnameOption[128] = "-ovolname=";
    hfs_to_utf8(vstat.name, volnameOption+10, 118);
    fuse_opt_add_arg(&args, volnameOption);
    fuse_opt_add_arg(&args, "-ofstypename=hfs");
    fuse_opt_add_arg(&args, "-olocal");
	//fuse_opt_add_arg(&args, "-oallow_root");
    fuse_opt_add_arg(&args, "-oallow_other");
	fuse_opt_add_arg(&args, "-odefer_permissions");
	fuse_opt_add_arg(&args, "-okill_on_unmount");
    char *fsnameOption = malloc(strlen(options.path)+10);
    strcpy(fsnameOption, "-ofsname=");
    strcat(fsnameOption, options.path);
    fuse_opt_add_arg(&args, fsnameOption);
    free(fsnameOption);
#endif
	
	// run fuse
	int ret = fuse_main(args.argc, args.argv, &FuseHFS_operations, NULL);
	
	// all good things come to an end
	hfs_umount(NULL);
	iconv_close(iconv_to_utf8);
	iconv_close(iconv_to_mac);
	free(options.path);
	free(options.encoding);
	fuse_opt_free_args(&args);
	printf("Goodbye!");
	return ret;
}
