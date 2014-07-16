fusehfs
=======

Goal: update FuseHFS (http://namedfork.net/fusehfs) to work with FUSE for OS X

This FUSE module was designed for MacFUSE, which is no longer maintained and doesn't work on newer versions of OS X. The FUSE for OS X (https://osxfuse.github.io) project picked up the FUSE-on-Mac baton, so that's the way to go from now on. Apple is nothing if not willing to drop support for outdated technologies, so Macs running OS X 10.6 onward can't write to HFS volumes. Evening reading seems to be broken as of 10.9, if not earlier.

I can't promise that I'll be able to get this working. I have a personal interest in restoring this functionality, and a very basic understanding of filesystems. I'm hoping to learn a lot through this project, so we'll see how it goes.

The original fusehfs code was published under GPL v2, so this version's code is too

--------------------------------
Status 7/8/2014

As described in this thread on the Emaculation forum: http://www.emaculation.com/forum/viewtopic.php?f=7&t=8181&p=48333#p48333

I'm able to make fusehfs work with FUSE for OS X, to an extent. My setup is as follows:

    •fusefs_hfs.fs is in /System/Library/Filesystems
    •/sbin/mount_fusefs_hfs is a symbolic link to /System/Library/Filesystems/fusefs_hfs.fs/Contents/Resources/fusefs_hfs
    (the fusehfs installer should put both of these things where they belong)
    •Mac OS X 10.9.4
    •FUSE for OS X 2.7.0 with MacFUSE compatibility layer installed

I can mount and unmount functional (read/write) HFS disk images from the shell, but definitely not using the graphical DiskImageMounter/DiskImages UI Agent/Disk Utility. These commands work for me:

$: hdiutil attach /path/to/disk/image
$: mount -t fusefs_hfs /dev/disk# /specify/a/mount/point
# where disk# is the value that hdiutil reports
# And I can safely unmount the volume with just:
$: umount /mount/point/you/specified

--------------------------------
Status 7/15/2014

Someone on the Emaculation forum speculated the problem was due to permissions, which is on the right track.

The problem occurs within FUSE for OS X, not within any of the code of fusehfs itself. The syscall mount() on line 938 [url=https://github.com/osxfuse/kext/blob/4a279b94df767db3fa0e2513155b9369ea7e3e90/mount/mount_osxfusefs.c]in this program[/url] fails due to EPERM, which I believe is due to the user lacking write access to the mount point in /Volumes. [url=https://github.com/osxfuse/fuse/blob/fuse-2.7/README]FUSE for OS X documentation[/url] indicates that the user must have write access to the mount point. It looks like my working copy of fusehfs on 10.6 with MacFUSE runs the fusefs_hfs program as root, which is not good if avoidable.

What's strange is that my workaround isn't subject to this. I can see that mount_fusefs_hfs is called with the same arguments by the same real/effective user and the exact same mount() call will work when mount_fusefs_hfs is called via my workaround, but not when mount_fusefs_hfs is called by double-clicking the disk image. Something must be different in the environment from which mount_fusefs_hfs is called, but I don't yet know what. I'm going to dig in to the [url=https://opensource.apple.com/source/diskdev_cmds/diskdev_cmds-572.1.1/mount.tproj/mount.c]source to mount[/url] to see what it does before calling the filesystem-specific program (in our case, mount_fusefs_hfs).

I'll also try to figure out how other filesystems like sshfs manage to mount in /Volumes without escalating to root. Requesting admin privileges from the user should work if all else fails, but that's really not a great option.
