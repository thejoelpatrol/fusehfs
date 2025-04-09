/*
 * main.c
 * FuseHFS
 *
 * This is the entry point of mount_fusefs_hfs.
 *
 * Rough order of operations when double clicking a disk image:
 * . Finder opens file path with /System/Library/CoreServices/DiskImageMounter.app
 * . DiskImageMounter opens image file RW
 * . diskimagescontroller creates a device
 * . diskimagescontroller asks diskimagesiod to attach the disk image to the device
 * . the kernel loads macFUSE kext, if not already
 * . diskarbitrationd becomes ready
 * . diskimagescontroller and DiskImageMounter quit
 * . diskarbitrationd learns that a disk device has appeared, keeps track of it, but calls it unreadable for now
 *      _DAMediaAppearedCallback()
 *      https://opensource.apple.com/source/DiskArbitration/DiskArbitration-342.140.1/diskarbitrationd/DAServer.c.auto.html
 * . diskarbitrationd checks FS_DIR_LOCATION (/System/Library/Filesystems) for loadable filesystems
 *      https://opensource.apple.com/source/DiskArbitration/DiskArbitration-342.140.1/diskarbitrationd/DASupport.c.auto.html
 *      https://opensource.apple.com/source/xnu/xnu-7195.141.2/bsd/sys/loadable_fs.h.auto.html
 * . diskarbitrationd checks ___FS_DEFAULT_DIR (/Library/Filesystems) for loadable filesystems
 *      https://opensource.apple.com/source/DiskArbitration/DiskArbitration-342.140.1/diskarbitrationd/DABase.h.auto.html
 * . diskarbitrationd uses the plist in every loadable fs to find its probe command and calls it
 *      in our case, # fusefs_hfs.util -p
 * . once a filesystem asserts compatibility, diskarbitrationd runs the repair command from the plist # hfsck -y
 *      https://opensource.apple.com/source/DiskArbitration/DiskArbitration-342.80.2/diskarbitrationd/DAFileSystem.c.auto.html
 *      __DAFileSystemProbeCallback(), __DAFileSystemProbeCallbackStage1(), __DAFileSystemProbeCallbackStage2()
 * . diskarbitrationd calls the mount command: # mount_fusefs_hfs $device $mountpoint
 *      https://opensource.apple.com/source/DiskArbitration/DiskArbitration-342.80.2/diskarbitrationd/DAMount.c.auto.html
 *      __DAMountWithArgumentsCallback(), __DAMountWithArgumentsCallbackStage1(), __DAMountWithArgumentsCallbackStage2()
 *      https://opensource.apple.com/source/DiskArbitration/DiskArbitration-342.80.2/diskarbitrationd/DAFileSystem.c.auto.html
 *      DAFileSystemMountWithArguments()
 * . main() uses hfs_mount() to check whether to mount readonly
 * . main() calls fuse_main(), which handles the rest of mounting and does not return until unmount
 * . macFUSE calls in to the struct fuse_operations FuseHFS_operations we gave it to handle all the actual HFS operations with the functions in fusefs_hfs.c
 *
 * For info on how to see what diskarbitrationd is doing to learn all this ^ see how_to_debug_diskarbitrationd.md
 *
 * Created by Zydeco on 27/2/2010.
 * Copyright 2010 namedfork.net. All rights reserved.
 *
 * Edited by Joel Cretan 7/19/2014
 * Edited by Joel Cretan 8/24/2022
 *
 * Still licensed under GPLv2: https://www.gnu.org/licenses/gpl-2.0.html
 */
#include "common.h"

#include <fuse/fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <iconv.h>
#include <stdlib.h>
#include <libhfs/hfs.h>

#include "log.h"
#include "fusefs_hfs.h"

#define FILENAME "[main.c]\t"
#define FUSEHFS_VERSION "0.1.5"
#define DEBUG

extern struct fuse_operations FuseHFS_operations;

struct fusehfs_options options = {
    .path =         NULL,
    .encoding =		NULL,
    .readonly =		0,
    .partition = 0,
};

enum {
	KEY_VERSION,
	KEY_HELP,
	KEY_ENCODING,
	KEY_READONLY,
	KEY_PARTITION,
};

static struct fuse_opt FuseHFS_opts[] = {
	FUSE_OPT_KEY("-V",			KEY_VERSION),
	FUSE_OPT_KEY("--version",	KEY_VERSION),
	FUSE_OPT_KEY("-h",			KEY_HELP),
	FUSE_OPT_KEY("--help",		KEY_HELP),
	FUSE_OPT_KEY("--encoding=",	KEY_ENCODING),
	FUSE_OPT_KEY("--readonly",	KEY_READONLY),
	FUSE_OPT_KEY("--partition=",	KEY_PARTITION),
	FUSE_OPT_END
};

static void log_fuse_call(struct fuse_args *args) {
    char buf[1024];
    for (int i = 0; i < args->argc; i++) {
        if (strlen(buf) + strlen(args->argv[i]) + 1 <= 1024) {
            strcat(buf, args->argv[i]);
            buf[strlen(buf) + 1] = 0;
            buf[strlen(buf)] = ' ';
        }
    }
    fprintf(stderr, FILENAME "Running fuse_main: fuse_main(%d, %s)\n", args->argc, buf);
    fflush(stderr);
}

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
			fprintf(stderr, "FuseHFS %s, (c)2010 namedfork.net namedfork.net\n", FUSEHFS_VERSION);
			exit(1);
		case KEY_HELP:
			fprintf(stderr, "usage: fusefs_hfs [fuse options] device mountpoint\n");
			exit(0);
		case KEY_READONLY:
			options.readonly = 1;
			return 0;
		case KEY_PARTITION:
			options.partition = strtoul(arg+12, 0, 0);
			return 0;
	}
	return 0;
}

char * iconv_convert(const char *src, const char *from, const char *to) {
	size_t inb = strlen(src);
	size_t outb = inb*4;
	char *out = malloc(outb+1);
	char *outp = out;
	
	// allocate conversion descriptor
	iconv_t cd = iconv_open(from, to);
	if (cd == (iconv_t)-1) return NULL;
	
	// convert
	if (iconv(cd, (char **restrict)&src, &inb, &outp, &outb) == (size_t)-1) {
		iconv_close(cd);
		free(out);
		return NULL;
	}
	*outp = '\0';
	
	// The End
	iconv_close(cd);
	return out;
}

#define ROOT_UID 0
static bool is_root() {
    int euid = geteuid();
    return euid == ROOT_UID;
}


int main(int argc, char* argv[], char* envp[], char** exec_path) {
	log_to_file();
    log_invoking_command(FILENAME, argc, argv);

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	
	bzero(&options, sizeof options);
	if (fuse_opt_parse(&args, NULL, FuseHFS_opts, FuseHFS_opt_proc))
        return 1;
	options.mountpoint = strdup(argv[1]);
	if (options.encoding == NULL)
        options.encoding = strdup("Macintosh");
	
	// mount volume
	hfsvolent vstat;
	int mode = options.readonly?HFS_MODE_RDONLY:HFS_MODE_ANY;
	if (NULL == hfs_mount(options.path, options.partition, mode)) {
		perror("hfs_mount");
		return 1;
	}
	if (-1 == hfs_vstat(NULL, &vstat)) {
		perror("hfs_vstat");
		return 1;
	}
	
	// is it read-only?
	if (vstat.flags & HFS_ISLOCKED) {
		options.readonly = 1;
		fuse_opt_add_arg(&args, "-oro");
	}
	fuse_opt_add_arg(&args, "-s");
	hfs_umount(NULL);

	// MacFUSE options
    char volnameOption[128] = "-ovolname=";
	char *volname = iconv_convert(vstat.name, options.encoding, "UTF-8");
	if (volname == NULL) {
		perror("iconv");
		return 1;
	}
    strncpy(volnameOption+10, volname, sizeof(volnameOption) - 10);
	free(volname);
    fuse_opt_add_arg(&args, volnameOption);
    fuse_opt_add_arg(&args, "-s");
    fuse_opt_add_arg(&args, "-ofstypename=hfs");
    if (is_root()) fuse_opt_add_arg(&args, "-oallow_other"); // this option requires privileges
    fuse_opt_add_arg(&args, "-odefer_permissions");
    char *fsnameOption = malloc(strlen(options.path)+10);
    strncpy(fsnameOption, "-ofsname=", strlen(options.path)+10);
    strncat(fsnameOption, options.path, strlen(options.path)+10);
    fuse_opt_add_arg(&args, fsnameOption);
    free(fsnameOption);
    //fuse_opt_add_arg(&args, "-debug");
    fuse_opt_add_arg(&args, "-olocal"); // full effect uncertain, but necessary to display it at as a local drive
                                        // rather than a server, which assures better unmounting (fully ejecting disk image)
                                        // https://github.com/osxfuse/osxfuse/wiki/Mount-options
     
	// run fuse
    log_fuse_call(&args);
    char *macfuse_mode = getenv("OSXFUSE_MACFUSE_MODE");
    fprintf(stderr, FILENAME "MacFUSE mode: %s\n", macfuse_mode);

	int ret = fuse_main(args.argc, args.argv, &FuseHFS_operations, &options);
	    
    log_to_file(); // macFUSE apparently messes with stderr so you have to set this again
    fprintf(stderr, FILENAME "Quitting fusefs_hfs, returning %d\n\n", ret);
    fflush(stderr);
    return ret;
}
