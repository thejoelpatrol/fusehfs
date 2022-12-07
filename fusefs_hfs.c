/*
 * fusefs_hfs.c
 * FuseHFS
 *
 * This file contains the meat of the macFUSE implementation, all
 * the functions that implement the FUSE API. It is largely unchanged
 * from zydeco's original v0.1.3
 *
 * &FuseHFS_operations is passed to fuse_main() and the callbacks are invoked
 * when doing operations like reading a directory, opening/writing files, etc.
 * Actually handling the HFS file system is mostly deferred to hfsutils, included
 * as a source library.
 * e.g. FuseHFS_open() calls hfs_open(), FuseHFS_unlink() calls hfs_delete(), etc
 *
 * Created by Zydeco on 2010-02-27.
 * Copyright 2010 namedfork.net. All rights reserved.
 * Edited by Joel Cretan 2022-08-24
 *
 * Licensed under GPLv2: https://www.gnu.org/licenses/gpl-2.0.html
 */
#include "common.h"

#include <fuse/fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/stat.h>
#include <libhfs/hfs.h>
#include <libhfs/apple.h>
#include <iconv.h>
#include <unistd.h>
#include <assert.h>
#include <libkern/OSByteOrder.h>
#include <sys/xattr.h>

#include "fusefs_hfs.h"
#include "log.h"

#define FILENAME "[fusefs_hfs.c]\t"

#ifdef DEBUG
#define dprintf(args...) printf(args)
#else
#define dprintf(fmt, args...)
#endif

// globals
iconv_t iconv_to_utf8, iconv_to_mac;
char _volname[HFS_MAX_VLEN+1];
int _readonly;

#pragma mark Character set conversion
char * hfs_to_utf8 (const char * in, char * out, size_t outlen) {
    size_t len = strlen(in);
    size_t outleft;
    char * outp = out;
    if (out == NULL) {
        outlen = (len*4)+1; // *3 is ok for MacRoman, what about Shift-JIS and others?
        out = malloc(outlen);
    }
    outleft = outlen-1;
    iconv(iconv_to_utf8, (char **restrict)&in, &len, &outp, &outleft);
    iconv(iconv_to_utf8, NULL, NULL, NULL, NULL);
    out[outlen-outleft-1] = '\0';
    
    // swap / and :
    for(outp=out;*outp;outp++) {
        if (*outp == ':') *outp = '/';
        else if (*outp == '/') *outp = ':';
    }
    
    return out;
}

char * utf8_to_hfs (const char * in) {
    size_t len, outlen, outleft;
    len = outleft = strlen(in);
    outlen = len+1;
    char * out = malloc(outlen);
    char * outp = out;
    iconv(iconv_to_mac, (char **restrict)&in, &len, &outp, &outleft);
    iconv(iconv_to_mac, NULL, NULL, NULL, NULL);
    out[outlen-outleft-1] = '\0';
    
    // swap / and :
    for(outp=out;*outp;outp++) {
        if (*outp == ':') *outp = '/';
        else if (*outp == '/') *outp = ':';
    }
    
    return out;
}

char * mkhfspath(const char *in) {
	assert(in[0] == '/');
	size_t len, vollen,outlen, outleft;
    len = outleft = strlen(in);
	vollen = strlen(_volname);
    outlen = vollen+len+1;
    char * out = malloc(outlen);
    char * outp = out + vollen;
	// prepend volume name
	strcpy(out, _volname);
	// convert path
    iconv(iconv_to_mac, (char **restrict)&in, &len, &outp, &outleft);
    iconv(iconv_to_mac, NULL, NULL, NULL, NULL);
    out[outlen-outleft-1] = '\0';
    
    // swap / and :
    for(outp=out+vollen;*outp;outp++) {
        if (*outp == ':') *outp = '/';
        else if (*outp == '/') *outp = ':';
    }
    
    return out;
}

#pragma mark Misc

static int dirent_to_stbuf(const hfsdirent *ent, struct stat *stbuf) {
	if (ent == NULL || stbuf == NULL) return -1;
	memset(stbuf, 0, sizeof(struct stat));
	stbuf->st_ino = ent->cnid;
	stbuf->st_mode = 0755;
	stbuf->st_atime = stbuf->st_mtime = ent->mddate;
	stbuf->st_ctime = ent->crdate;
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	if (ent->flags & HFS_ISDIR) {
		// directory
		stbuf->st_mode |= S_IFDIR;
		stbuf->st_nlink = 2 + ent->u.dir.valence;
	} else {
		// regular file
		stbuf->st_mode |= S_IFREG;
		stbuf->st_nlink = 1;
		stbuf->st_size = ent->u.file.dsize;
	}
	return 0;
}

#pragma mark FUSE Callbacks

static int FuseHFS_fgetattr(const char *path, struct stat *stbuf,
                  struct fuse_file_info *fi) {
	hfsdirent ent;
	
	if (fi && (hfs_fstat((hfsfile*)fi->fh, &ent) == 0)) {
		// open file
		dirent_to_stbuf(&ent, stbuf);
		return 0;
	}
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// get file info
	
	if (hfs_stat(NULL, hfspath, &ent) == 0) {
		// file
		dirent_to_stbuf(&ent, stbuf);
		free(hfspath);
		return 0;
	}
	
	dprintf("fgetattr: ENOENT (%s)\n", path);
	free(hfspath);
	return -ENOENT;
}

static int FuseHFS_getattr(const char *path, struct stat *stbuf) {
	return FuseHFS_fgetattr(path, stbuf, NULL);
}

static int FuseHFS_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
	dprintf("readdir %s\n", path);
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// default directories
	filler(buf, ".", NULL, 0);           /* Current directory (.)  */
	filler(buf, "..", NULL, 0);          /* Parent directory (..)  */
	
	// open directory
	hfsdir *dir = hfs_opendir(NULL, hfspath);
	if (dir == NULL) {
		free(hfspath);
		dprintf("readdir: ENOENT\n");
		return -ENOENT;
	}
	
	// read contents
	hfsdirent ent;
	struct stat stbuf;
	char dname[4*HFS_MAX_FLEN+3];
	while (hfs_readdir(dir, &ent) == 0) {
		// File or directory
		dirent_to_stbuf(&ent, &stbuf);
		hfs_to_utf8(ent.name, dname, 4*HFS_MAX_FLEN);
		filler(buf, dname, &stbuf, 0);
	}
	
	// close
	hfs_closedir(dir);
	free(hfspath);
	return 0;
}

static int FuseHFS_mknod(const char *path, mode_t mode, dev_t rdev) {
	dprintf("mknod %s\n", path);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// open file
	hfsfile *file;
	if ((file = hfs_create(NULL, hfspath, "TEXT", "FUSE"))) {
		// file
		hfs_close(file);
		hfs_flush(NULL);
		free(hfspath);
		return 0;
	}
	
	dprintf("mknod: EPERM\n");
	free(hfspath);
	return -EPERM;
}

static int FuseHFS_mkdir(const char *path, mode_t mode) {
	dprintf("mkdir %s\n", path);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	if (hfs_mkdir(NULL, hfspath) == -1) {
		free(hfspath);
		perror("mkdir");
		return -errno;
	}
	
	free(hfspath);
	return 0;
}

static int FuseHFS_unlink(const char *path) {
	dprintf("unlink %s\n", path);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// check that file exists
	hfsdirent ent;
	if (hfs_stat(NULL, hfspath, &ent) == -1) {
		free(hfspath);
		dprintf("unlink: ENOENT\n");
		return -ENOENT;
	}
	
	// check that it's a file
	if (ent.flags & HFS_ISDIR) {
		free(hfspath);
		dprintf("unlink: EISDIR\n");
		return -EISDIR;
	}
	
	// delete it
	if (hfs_delete(NULL, hfspath) == -1) {
		free(hfspath);
		perror("unlink(2)");
		return -errno;
	}
	
	free(hfspath);
	return 0;
}

static int FuseHFS_rmdir(const char *path) {
	dprintf("rmdir %s\n", path);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// check that file exists
	hfsdirent ent;
	if (hfs_stat(NULL, hfspath, &ent) == -1) {
		free(hfspath);
		dprintf("rmdir: ENOENT\n");
		return -ENOENT;
	}
	
	// check that it's a directory
	if (!(ent.flags & HFS_ISDIR)) {
		free(hfspath);
		dprintf("rmdir: ENOTDIR\n");
		return -ENOTDIR;
	}
	
	// delete it
	if (hfs_rmdir(NULL, hfspath) == -1) {
		free(hfspath);
		perror("rmdir(2)");
		return -errno;
	}
	
	free(hfspath);
	return 0;
}

static int FuseHFS_rename(const char *from, const char *to) {
	dprintf("rename %s %s\n", from, to);
	if (_readonly) return -EPERM;
	
	// convert to hfs paths
	char *hfspath1 = mkhfspath(from);
	char *hfspath2 = mkhfspath(to);
	
	// delete destination file if it exists
	hfsdirent ent;
	if (hfs_stat(NULL, hfspath2, &ent) == 0)
		if (!(ent.flags & HFS_ISDIR)) hfs_delete(NULL, hfspath2);
	
	// rename
	if (hfs_rename(NULL, hfspath1, hfspath2) != 0) {
		free(hfspath1);
		free(hfspath2);
		perror("hfs_rename");
		return -errno;
	}
	
	// bless parent folder if it's a system file
	if (hfs_stat(NULL, hfspath2, &ent) == -1) {
		free(hfspath1);
		free(hfspath2);
		return -ENOENT;
	}
	
	if ((strcmp(ent.u.file.type, "zsys") == 0) && (strcmp(ent.u.file.creator, "MACS") == 0) && (strcmp(ent.name, "System") == 0)) {
		// bless
		dprintf("rename: blessing folder %lu\n", ent.parid);
		hfsvolent volent;
		hfs_vstat(NULL, &volent);
		volent.blessed = ent.parid;
		hfs_vsetattr(NULL, &volent);
	}
	
	// success
	free(hfspath1);
	free(hfspath2);
	return 0;
}

static int FuseHFS_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	dprintf("create %s\n", path);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// open file
	hfsfile *file;
	if ((file = hfs_create(NULL, hfspath, "TEXT", "FUSE"))) {
		// close and reopen, because it won't exist until it's closed
		hfs_close(file);
		file = hfs_open(NULL, hfspath);
		fi->fh = (uint64_t)file;
		free(hfspath);
		return 0;
	}
	
	free(hfspath);
	perror("hfs_create");
	return -errno;
}

static int FuseHFS_open(const char *path, struct fuse_file_info *fi) {
	dprintf("open %s\n", path);
	// apparently, MacFUSE won't open the same file more than once. This won't break if it stays this way.
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// open file
	hfsfile *file = NULL;
	if ((file = hfs_open(NULL, hfspath))) {
		// file
		fi->fh = (uint64_t)file;
		free(hfspath);
		return 0;
	}
	
	free(hfspath);
	perror("hfs_open");
	return -errno;
}

static int FuseHFS_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
	dprintf("read %s\n", path);
	
	hfsfile *file = (hfsfile*)fi->fh;
	hfs_setfork(file, 0);
	hfs_seek(file, offset, SEEK_SET);
	return hfs_read(file, buf, size);
}

static int FuseHFS_write(const char *path, const char *buf, size_t size,
               off_t offset, struct fuse_file_info *fi) {
	dprintf("write %s\n", path);
	if (_readonly) return -EPERM;
	
	hfsfile *file = (hfsfile*)fi->fh;
	hfs_setfork(file, 0);
	hfs_seek(file, offset, SEEK_SET);
	return (hfs_write(file, buf, size));
}

static int FuseHFS_statfs(const char *path, struct statvfs *stbuf) {
	memset(stbuf, 0, sizeof(*stbuf));
	hfsvolent vstat;
	hfs_vstat(NULL, &vstat);
	
	stbuf->f_bsize = stbuf->f_frsize = vstat.alblocksz;
	stbuf->f_blocks = vstat.totbytes / vstat.alblocksz;
	stbuf->f_bfree = stbuf->f_bavail = vstat.freebytes / vstat.alblocksz;
	 
	stbuf->f_files = vstat.numfiles + vstat.numdirs + 1;
	stbuf->f_namemax = HFS_MAX_FLEN;
	return 0;
}

static int FuseHFS_flush(const char *path, struct fuse_file_info *fi) {
	hfs_flush(NULL);
	return 0;
}

static int FuseHFS_release(const char *path, struct fuse_file_info *fi) {
	dprintf("close %s\n", path);
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	hfsfile *file = (hfsfile*)fi->fh;
	hfs_setfork(file, 0);
	hfs_close(file);
	free(hfspath);
	return 0;
}

void * FuseHFS_init(struct fuse_conn_info *conn) {
	struct fuse_context *cntx=fuse_get_context();
	struct fusehfs_options *options = cntx->private_data;
	
#if (__FreeBSD__ >= 10)
	FUSE_ENABLE_SETVOLNAME(conn); // this actually doesn't do anything
	FUSE_ENABLE_XTIMES(conn); // and apparently this doesn't either
#endif
	
	
#ifdef DEBUG
	//char logfn[128];
	//sprintf(logfn, "/fusefs_hfs/FuseHFS.%d.log", getpid());
	//stderr = freopen(logfn, "a", stderr);
	fprintf(stderr, "FuseHFS_init\n");
	fflush(stderr);
#endif
	
	// create iconv
	iconv_to_utf8 = iconv_open("UTF-8", options->encoding);
	if (iconv_to_utf8 == (iconv_t)-1) {
		perror("iconv_open");
		exit(1);
	}
	iconv_to_mac = iconv_open(options->encoding, "UTF-8");
	if (iconv_to_mac == (iconv_t)-1) {
		perror("iconv_open");
		exit(1);
	}
	
	// mount volume
	int mode = options->readonly?HFS_MODE_RDONLY:HFS_MODE_ANY;
	if (NULL == hfs_mount(options->path, options->partition, mode)) {
		perror("hfs_mount");
		exit(1);
	}
	
	// initialize some globals
	_readonly = options->readonly;
	hfsvolent vstat;
	hfs_vstat(NULL, &vstat);
	strcpy(_volname, vstat.name);
	
	return NULL;
}

void FuseHFS_destroy(void *userdata) {
	dprintf("FuseHFS_destroy\n");
	iconv_close(iconv_to_mac);
	iconv_close(iconv_to_utf8);
	hfs_umountall();
}

static int FuseHFS_listxattr(const char *path, char *list, size_t size) {
	dprintf("listxattr %s %p %lu\n", path, list, size);
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// find file
	hfsdirent ent;
	if (hfs_stat(NULL, hfspath, &ent) == -1) {
		free(hfspath);
		return -ENOENT;
	}
	free(hfspath);
	
	int needSize = sizeof XATTR_FINDERINFO_NAME;
	int haveRsrcFork = 0;
	if ((!(ent.flags & HFS_ISDIR)) && ent.u.file.rsize) {
		needSize += sizeof XATTR_RESOURCEFORK_NAME;
		haveRsrcFork = 1;
	}
	if (list == NULL) return needSize;
	if (size < needSize) return -ERANGE;
	
	bzero(list, size);
	strcpy(list, XATTR_FINDERINFO_NAME);
	if (haveRsrcFork) strcpy(list+sizeof XATTR_FINDERINFO_NAME, XATTR_RESOURCEFORK_NAME);
	
	return needSize;
}

static int FuseHFS_getxattr(const char *path, const char *name, char *value, size_t size,
				uint32_t position) {
	//dprintf("getxattr %s %s %p %lu %u\n", path, name, value, size, position);
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// find file
	hfsdirent ent;
	if (hfs_stat(NULL, hfspath, &ent) == -1) {
		free(hfspath);
		return -ENOENT;
	}
	
	
	if (strcmp(name, XATTR_FINDERINFO_NAME) == 0) {
		if (value == NULL) {
			free(hfspath);
			return 32;
		}
		if (size < 32) {
			free(hfspath);
			return -ERANGE;
		}
		// return finder info
		if (ent.flags & HFS_ISDIR) {
			// directory info
			OSWriteBigInt16(value, 0, ent.u.dir.rect.top);
			OSWriteBigInt16(value, 2, ent.u.dir.rect.left);
			OSWriteBigInt16(value, 4, ent.u.dir.rect.bottom);
			OSWriteBigInt16(value, 6, ent.u.dir.rect.right);
			OSWriteBigInt16(value, 8, ent.fdflags);
			OSWriteBigInt16(value, 10, ent.fdlocation.v);
			OSWriteBigInt16(value, 12, ent.fdlocation.h);
			OSWriteBigInt16(value, 14, ent.u.dir.view);
			// DXInfo
			OSWriteBigInt16(value, 16, ((DXInfo*)(ent.u.dir.xinfo))->frScroll.v);
			OSWriteBigInt16(value, 18, ((DXInfo*)(ent.u.dir.xinfo))->frScroll.h);
			OSWriteBigInt32(value, 20, ((DXInfo*)(ent.u.dir.xinfo))->frOpenChain);
			OSWriteBigInt16(value, 24, ((DXInfo*)(ent.u.dir.xinfo))->frUnused);
			OSWriteBigInt16(value, 26, ((DXInfo*)(ent.u.dir.xinfo))->frComment);
			OSWriteBigInt32(value, 28, ((DXInfo*)(ent.u.dir.xinfo))->frPutAway);		
		} else {
			// file info
			memcpy(value, ent.u.file.type, 4);
			memcpy(value+4, ent.u.file.creator, 4);
			OSWriteBigInt16(value, 8, ent.fdflags);
			OSWriteBigInt16(value, 10, ent.fdlocation.v);
			OSWriteBigInt16(value, 12, ent.fdlocation.h);
			OSWriteBigInt16(value, 14, ent.u.file.window);
			// FXInfo
			OSWriteBigInt16(value, 16, ((FXInfo*)(ent.u.file.xinfo))->fdIconID);
			OSWriteBigInt16(value, 18, ((FXInfo*)(ent.u.file.xinfo))->fdUnused[0]);
			OSWriteBigInt16(value, 20, ((FXInfo*)(ent.u.file.xinfo))->fdUnused[1]);
			OSWriteBigInt16(value, 22, ((FXInfo*)(ent.u.file.xinfo))->fdUnused[2]);
			OSWriteBigInt16(value, 24, ((FXInfo*)(ent.u.file.xinfo))->fdUnused[3]);
			OSWriteBigInt16(value, 26, ((FXInfo*)(ent.u.file.xinfo))->fdComment);
			OSWriteBigInt32(value, 28, ((FXInfo*)(ent.u.file.xinfo))->fdPutAway);
		}
		free(hfspath);
		return 32;
	} else if (strcmp(name, XATTR_RESOURCEFORK_NAME) == 0 && (!(ent.flags & HFS_ISDIR)) && ent.u.file.rsize) {
		// resource fork
		if (value == NULL) {
			free(hfspath);
			return ent.u.file.rsize-position;
		}
		int bw = ent.u.file.rsize-position;
		if (bw > size) bw = size;
		// copy resource fork
		hfsfile *fp = hfs_open(NULL, hfspath);
		hfs_setfork(fp, 1);
		hfs_seek(fp, position, SEEK_SET);
		hfs_read(fp, value, bw);
		hfs_close(fp);
		// the end
		free(hfspath);
		return bw;
	}
	
	free(hfspath);
	dprintf("getxattr: ENOATTR\n");
	return -ENOATTR;
}

static int FuseHFS_setxattr(const char *path, const char *name, const char *value,
				  size_t size, int flags, uint32_t position) {
	dprintf("setxattr %s %s %p %lu %02x %u\n", path, name, value, size, flags, position);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// find file
	hfsdirent ent;
	if (hfs_stat(NULL, hfspath, &ent) == -1) {
		free(hfspath);
		return -ENOENT;
	}
	
	if (strcmp(name, XATTR_FINDERINFO_NAME) == 0) {
		if (size != 32) {
			dprintf("setxattr: finder info is not 32 bytes\n");
			free(hfspath);
			return -ERANGE;
		}
		// write finder info to dirent
		if (ent.flags & HFS_ISDIR) {
			// directory
			ent.u.dir.rect.top =		OSReadBigInt16(value, 0);
			ent.u.dir.rect.left =		OSReadBigInt16(value, 2);
			ent.u.dir.rect.bottom =		OSReadBigInt16(value, 4);
			ent.u.dir.rect.right =		OSReadBigInt16(value, 6);
			ent.fdflags =				OSReadBigInt16(value, 8);
			ent.fdlocation.v =			OSReadBigInt16(value, 10);
			ent.fdlocation.h =			OSReadBigInt16(value, 12);
			ent.u.dir.view =			OSReadBigInt16(value, 14);
			// DXInfo
			((DXInfo*)(ent.u.dir.xinfo))->frScroll.v   = OSReadBigInt16(value, 16);
			((DXInfo*)(ent.u.dir.xinfo))->frScroll.h   = OSReadBigInt16(value, 18);
			((DXInfo*)(ent.u.dir.xinfo))->frOpenChain  = OSReadBigInt32(value, 20);
			((DXInfo*)(ent.u.dir.xinfo))->frUnused     = OSReadBigInt16(value, 24);
			((DXInfo*)(ent.u.dir.xinfo))->frComment    = OSReadBigInt16(value, 26);
			((DXInfo*)(ent.u.dir.xinfo))->frPutAway    = OSReadBigInt32(value, 28);
		} else {
			// regular file
			memcpy(ent.u.file.type, value, 4);
			memcpy(ent.u.file.creator, value+4, 4);
			ent.u.file.type[4] = ent.u.file.type[4] = '\0';
			ent.fdflags       = OSReadBigInt16(value, 8);
			ent.fdlocation.v  = OSReadBigInt16(value, 10);
			ent.fdlocation.h  = OSReadBigInt16(value, 12);
			ent.u.file.window = OSReadBigInt16(value, 14);
			// FXInfo
			((FXInfo*)(ent.u.file.xinfo))->fdIconID    = OSReadBigInt16(value, 16);
			((FXInfo*)(ent.u.file.xinfo))->fdUnused[0] = OSReadBigInt16(value, 18);
			((FXInfo*)(ent.u.file.xinfo))->fdUnused[1] = OSReadBigInt16(value, 20);
			((FXInfo*)(ent.u.file.xinfo))->fdUnused[2] = OSReadBigInt16(value, 22);
			((FXInfo*)(ent.u.file.xinfo))->fdUnused[3] = OSReadBigInt16(value, 24);
			((FXInfo*)(ent.u.file.xinfo))->fdComment   = OSReadBigInt16(value, 26);
			((FXInfo*)(ent.u.file.xinfo))->fdPutAway   = OSReadBigInt32(value, 28);
			// bless parent folder if it's a system file
			if ((strcmp(ent.u.file.type, "zsys") == 0) && (strcmp(ent.u.file.creator, "MACS") == 0) && (strcmp(ent.name, "System") == 0)) {
				// bless
				dprintf("setxattr: blessing folder %lu\n", ent.parid);
				hfsvolent volent;
				hfs_vstat(NULL, &volent);
				volent.blessed = ent.parid;
				hfs_vsetattr(NULL, &volent);
			}
		}
		// update file
		hfs_setattr(NULL, hfspath, &ent);
		free(hfspath);
		return 0;
	} else if (strcmp(name, XATTR_RESOURCEFORK_NAME) == 0 && (!(ent.flags & HFS_ISDIR))) {
		// resource fork
		// TODO: how are resource forks truncated?
		hfsfile *fp = hfs_open(NULL, hfspath);
		hfs_setfork(fp, 1);
		hfs_seek(fp, position, SEEK_SET);
		hfs_write(fp, value, size);
		hfs_close(fp);
		// the end
		free(hfspath);
		return 0;
	} else {
		free(hfspath);
		return 0;
	}
	
	free(hfspath);
	return -ENOATTR;
	
}

static int FuseHFS_removexattr(const char *path, const char *name) {
	dprintf("removexattr %s %s\n", path, name);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// find file
	hfsdirent ent;
	if (hfs_stat(NULL, hfspath, &ent) == -1) {
		free(hfspath);
		return -ENOENT;
	}
	
	if (strcmp(name, XATTR_FINDERINFO_NAME) == 0) {
		free(hfspath);
		// not really removing it
		return 0;
	} else if (strcmp(name, XATTR_RESOURCEFORK_NAME) == 0 && (!(ent.flags & HFS_ISDIR))) {
		// resource fork
		hfsfile *fp = hfs_open(NULL, hfspath);
		hfs_setfork(fp, 1);
		hfs_seek(fp, 0, SEEK_SET);
		hfs_truncate(fp, 0);
		hfs_close(fp);
		free(hfspath);
		return 0;
	}
	
	free(hfspath);
	return -ENOATTR;	
}

static int FuseHFS_chmod (const char *path, mode_t newmod) {
	dprintf("chmod %s %o\n", path, newmod);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// check that file exists
	hfsdirent ent;
	if (hfs_stat(NULL, hfspath, &ent) == -1) {
		free(hfspath);
		return -ENOENT;
	}
	
	free(hfspath);
	return 0;
}

static int FuseHFS_chown (const char *path, uid_t newuid, gid_t newgid) {
	dprintf("chown %s %d %d\n", path, newuid, newgid);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// check that file exists
	hfsdirent ent;
	if (hfs_stat(NULL, hfspath, &ent) == -1) {
		free(hfspath);
		perror("chown");
		return -errno;
	}
	
	free(hfspath);
	return 0;
}

static int FuseHFS_ftruncate (const char *path, off_t length, struct fuse_file_info *fi) {
	dprintf("ftruncate %s %lu\n", path, length);
	if (_readonly) return -EPERM;
	
	hfsfile *file = (hfsfile*)fi->fh;
	if (hfs_truncate(file, length) == -1) {
		perror("truncate");
		return -errno;
	}
	return 0;
}

static int FuseHFS_truncate (const char *path, off_t length) {
	dprintf("truncate %s %lu\n", path, length);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	hfsfile *file = hfs_open(NULL, hfspath);
	free(hfspath);
	if (file == NULL) return -errno;
	if (hfs_truncate(file, length) == -1) {
		hfs_close(file);
		perror("truncate");
		return -errno;
	}
	hfs_close(file);
	return 0;
}

static int FuseHFS_utimens (const char *path, const struct timespec tv[2]) {
	dprintf("utimens %s\n", path);
	return 0;
}

#if (__FreeBSD__ >= 10)

static int FuseHFS_setvolname (const char *name) {
	dprintf("setvolname %s\n", name);
	if (_readonly) return -EPERM;
	
	// convert to hfs
	char *hfsname = utf8_to_hfs(name);
	if (strlen(hfsname) > HFS_MAX_VLEN) {
		free(hfsname);
		return -E2BIG;
	}
	
	// rename volume
	if (hfs_rename(NULL, _volname, hfsname)) return -EPERM;
	// update
	strcpy(hfsname, _volname);
	return 0;
}

static int FuseHFS_getxtimes(const char *path, struct timespec *bkuptime,
                   struct timespec *crtime) {
	dprintf("getxtimes %s\n", path);
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	
	// get file info
	hfsdirent ent;
	
	if (hfs_stat(NULL, hfspath, &ent) == 0) {
		// file
		crtime->tv_sec = ent.crdate;
		crtime->tv_nsec = 0;
		bkuptime->tv_sec = ent.bkdate;
		bkuptime->tv_nsec = 0;
		free(hfspath);
		return 0;
	}
	
	free(hfspath);
	perror("getxtimes:hfs_stat");
	return -errno;
}

static int FuseHFS_setbkuptime (const char *path, const struct timespec *tv) {
	dprintf("setbkuptime %s\n", path);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	int err;
	
	// get file info
	hfsdirent ent;
	
	if (hfs_stat(NULL, hfspath, &ent) == 0) {
		ent.bkdate = tv->tv_sec;
		err = hfs_setattr(NULL, hfspath, &ent);
		free(hfspath);
		if (!err) return 0;
		perror("hfs_setattr");
		return -errno;
	}
	
	free(hfspath);
	perror("hfs_stat");
	return -errno;
}

static int FuseHFS_setchgtime (const char *path, const struct timespec *tv) {
	dprintf("setchgtime %s\n", path);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	int err;
	
	// get file info
	hfsdirent ent;
	
	if (hfs_stat(NULL, hfspath, &ent) == 0) {
		ent.mddate = tv->tv_sec;
		err = hfs_setattr(NULL, hfspath, &ent);
		free(hfspath);
		if (!err) return 0;
		perror("hfs_setattr");
		return -errno;
	}
	
	free(hfspath);
	perror("hfs_stat");
	return -errno;
}

static int FuseHFS_setcrtime (const char *path, const struct timespec *tv) {
	dprintf("setcrtime %s\n", path);
	if (_readonly) return -EPERM;
	
	// convert to hfs path
	char *hfspath = mkhfspath(path);
	if (hfspath == NULL) return -ENOENT;
	int err;
	
	// get file info
	hfsdirent ent;
	
	if (hfs_stat(NULL, hfspath, &ent) == 0) {
		ent.crdate = tv->tv_sec;
		err = hfs_setattr(NULL, hfspath, &ent);
		free(hfspath);
		if (!err) return 0;
		perror("hfs_setattr");
		return -errno;
	}
	
	free(hfspath);
	perror("hfs_stat");
	return -errno;
}

#endif
struct fuse_operations FuseHFS_operations = {
	.init        = FuseHFS_init,
	.destroy     = FuseHFS_destroy,
	.getattr     = FuseHFS_getattr,
	.fgetattr    = FuseHFS_fgetattr,
	.readdir     = FuseHFS_readdir,
	.mknod       = FuseHFS_mknod,
	.mkdir       = FuseHFS_mkdir,
	.unlink      = FuseHFS_unlink,
	.rmdir       = FuseHFS_rmdir,
	.rename      = FuseHFS_rename,
	.create      = FuseHFS_create,
	.open        = FuseHFS_open,
	.read        = FuseHFS_read,
	.write       = FuseHFS_write,
	.statfs      = FuseHFS_statfs,
	//.flush       = FuseHFS_flush,
	.release     = FuseHFS_release,
	//.fsync       = FuseHFS_fsync,
	.listxattr   = FuseHFS_listxattr,
	.getxattr    = FuseHFS_getxattr,
	.setxattr    = FuseHFS_setxattr,
	.removexattr = FuseHFS_removexattr,
	.truncate    = FuseHFS_truncate,
	.ftruncate   = FuseHFS_ftruncate,
	.chmod		 = FuseHFS_chmod,
	.chown       = FuseHFS_chown,
	.utimens     = FuseHFS_utimens,
#if (__FreeBSD__ >= 10)
	.setvolname  = FuseHFS_setvolname,
	.getxtimes   = FuseHFS_getxtimes,
	.setcrtime   = FuseHFS_setcrtime,
	.setchgtime  = FuseHFS_setchgtime,
	.setbkuptime = FuseHFS_setbkuptime,
#endif
};
