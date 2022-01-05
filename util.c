/*
 *  util.c
 *  FuseHFS
 *
 *  Created by Zydeco on 11/3/2010.
 *  Copyright 2010 namedfork.net. All rights reserved.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libhfs/libhfs.h>
#include <sys/loadable_fs.h>
#include <sys/stat.h>
#include <iconv.h>
#include <sys/errno.h>

/* no longer supported in xnu 3247 / osx 10.11  */
/* from xnu/bsd/sys/loadable_fs.h.auto.html     */
#define    FSUC_INITIALIZE        'i'    /* initialize FS */

int usage() {
	fprintf(stderr, "usage: fusefs_hfs.util [-p|-m|-i] <options>\n");
	return EXIT_FAILURE;
}

// TODO: this is old
/*int have_macfuse() {
	struct stat us;
	return (stat("/usr/local/lib/libfuse_ino64.dylib", &us) == 0);
}*/

#define HFSPLUS_SIGWORD	0x482b /* 'H+' */
#define HFSX_SIGWORD	0x482b /* 'HX' */

int probe (const char *device, int removable, int readonly) {
	char *path;
	int ret = FSUR_RECOGNIZED;
	asprintf(&path, "/dev/%s", device);
	hfsvol * vol = hfs_mount(path, 0, HFS_MODE_RDONLY);
	free(path);
	if (vol == NULL) return FSUR_UNRECOGNIZED;

	/* Refuse to mount HFS+ wrapper volume */
	int embed = vol->mdb.drEmbedSigWord;
	if (embed == HFSPLUS_SIGWORD || embed == HFSX_SIGWORD) {
		ret = FSUR_UNRECOGNIZED;
	} else {
		hfsvolent ent;
		if (hfs_vstat(vol, &ent) == 0)
			printf("%s\n", ent.name); // TODO: convert to UTF8
	}
	hfs_umount(vol);
	return ret;
}

int initialize (const char *device, const char *label) {
	// convert label to MacRoman
	char volname[HFS_MAX_VLEN+1];
	size_t len = strlen(label);
	size_t outleft = HFS_MAX_VLEN;
	char *outp = volname;
	iconv_t conv = iconv_open("Macintosh", "UTF-8");
	iconv(conv, (char **restrict)&label, &len, &outp, &outleft);
	volname[HFS_MAX_VLEN-outleft] = '\0';
	volname[HFS_MAX_VLEN] = '\0';
	iconv_close(conv);
	
	// format
	if (hfs_format(device, 0, 0, volname, 0, NULL)) {
		return FSUR_IO_FAIL;
	}
	return EXIT_SUCCESS;
}

int mount (const char *device, const char *mountpoint) {
	char *cmd;
	asprintf(&cmd, "/System/Library/Filesystems/fusefs_hfs.fs/Contents/Resources/fuse_wait \"%s\" %d /System/Library/Filesystems/fusefs_hfs.fs/Contents/Resources/fusefs_hfs \"%s\" \"%s\"", mountpoint, 5, device, mountpoint);
	int ret = system(cmd);
	free(cmd);
	return ret?FSUR_IO_FAIL:FSUR_IO_SUCCESS;
}

int main (int argc, char * argv[], char * envp[], char * apple[]) {	
	// check arguments
	if (argc < 3) return usage();
	int ret = 0;
	switch (argv[1][1]) {
		case FSUC_PROBE:
			ret = probe(argv[2], !strcmp(argv[3],DEVICE_REMOVABLE), !strcmp(argv[4],DEVICE_READONLY));
			break;
		case FSUC_INITIALIZE: {
			int larg = 2;
			const char *label, *device;
			if (strcmp(argv[2], "-v") == 0) larg = 3;
			label = argv[larg];
			device = argv[larg+1];
			ret = initialize(device, label);
			break; }
		case FSUC_MOUNT:
			ret = mount(argv[argc-2], argv[argc-1]);
			break;
		case 'k': // get UUID
		case 's': // set UUID
			ret = FSUR_INVAL;
			break;
		default:
			ret = FSUR_INVAL;
			break;
	}
	return ret;
}
