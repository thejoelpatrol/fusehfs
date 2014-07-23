fusehfs
=======

Goal: update [FuseHFS](http://namedfork.net/fusehfs) to work with [FUSE for OS X](https://osxfuse.github.io/) on 10.9

This FUSE module was designed for MacFUSE, which is no longer maintained and doesn't work on newer versions of OS X. The FUSE for OS X (https://osxfuse.github.io) project picked up the FUSE-on-Mac baton, so that's the way to go from now on. Apple is nothing if not willing to drop support for outdated technologies, so Macs running OS X 10.6 onward can't write to HFS volumes. Even reading seems to be broken as of 10.9.

The original fusehfs code was published under GPL v2, so this version's code is too.

--------------------------------
#### Status 7/19/2014

The problem is the [-oallow-other option](https://code.google.com/p/macfuse/wiki/OPTIONS). This option can only be used by privileged users. On older systems, the program mount\_fusefs did in fact run as root, but now mount\_osxfusefs does not, which is for the better. It seems to me that users who are in group 80 should have been able to use this option, but evidently not. Group 80 (admin) is the default "MacFUSE admin" and "OSXFUSE admin" group, so FUSE should run as a member of this group, but it seems to prefer 20 (staff) for some reason. My understanding of the option may be wrong anyway, if wheel is actually required. For now, leaving out this option will allow FuseHFS to work for the user who mounts the volume. Considering the somewhat niche use of this filesystem these days, this is probably OK for now.

I changed fusehfs (specifically, mount\_fusefs\_hfs) to only attempt this option if the user is root. There might be a way to add this back in later for all users, but for now omitting this function for non-root users is better than not functioning at all.

--------------------------------
#### Status 7/18/2014


The problem does not seem to lie within the mount program (the one that calls mount\_fusefs\_hfs, not the syscall). When called from the shell, which works, it receives the same arguments as when called by hdiutil, which does not work without root. Its execution appears to proceed in the same way within lldb, no matter where it is called from or if its caller hdiutil is run by root. Its real and effective user IDs are the same both ways. If it is changing something that affects the eventual mount() syscall, I can't see what it is.

The problem does not seem to be improper permissions on the mount point in /Volumes, or at least that's not the only problem. Running hdiutil as root does allow the critical mount() call (and everything else along the way) to work correctly, but simply adding read/write permissions to the mount point does not have the same effect. I used lldb to pause the various processes involved (hdiutil, mount, mount\_osxfusefs) at different points and tried setting permissions and then allowing them to continue, with no apparent effect. I also found that when *only* mount\_osxfusefs is run with an effective UID of root, the mount() call still does not work. So there is more to this failing syscall than meets the eye.

--------------------------------
#### Status 7/15/2014


Someone on the Emaculation forum speculated the problem was due to permissions, which is on the right track.

The problem occurs within FUSE for OS X, not within any of the code of fusehfs itself. The syscall mount() on line 938 [in this program](https://github.com/osxfuse/kext/blob/4a279b94df767db3fa0e2513155b9369ea7e3e90/mount/mount_osxfusefs.c) fails due to EPERM, which I believe is due to the user lacking write access to the mount point in /Volumes. The [FUSE for OS X documentation](https://github.com/osxfuse/fuse/blob/fuse-2.7/README) indicates that the user must have write access to the mount point. It looks like my working copy of fusehfs on 10.6 with MacFUSE runs the fusefs_hfs program as root, which is not good if avoidable.

What's strange is that my workaround isn't subject to this. I can see that mount\_fusefs\_hfs is called with the same arguments by the same real/effective user and the exact same mount() call will work when mount_fusefs_hfs is called via my workaround, but not when mount\_fusefs\_hfs is called by double-clicking the disk image. Something must be different in the environment from which mount\_fusefs\_hfs is called, but I don't yet know what. I'm going to dig in to the [source to mount](https://opensource.apple.com/source/diskdev_cmds/diskdev_cmds-572.1.1/mount.tproj/mount.c) to see what it does before calling the filesystem-specific program (in our case, mount\_fusefs\_hfs).

I'll also try to figure out how other filesystems like sshfs manage to mount in /Volumes without escalating to root. Requesting privileges from the user should work if all else fails, but that's really not a great option.

--------------------------------
#### Status 7/8/2014


As described in this thread on the Emaculation forum: http://www.emaculation.com/forum/viewtopic.php?f=7&t=8181&p=48333#p48333

I'm able to make fusehfs work with FUSE for OS X to an extent, with no modifications. My setup is as follows:

    •fusefs_hfs.fs is in /System/Library/Filesystems
    •/sbin/mount_fusefs_hfs is a symbolic link to /System/Library/Filesystems/fusefs_hfs.fs/Contents/Resources/fusefs_hfs
    (the fusehfs installer should put both of these things where they belong)
    •Mac OS X 10.9.4
    •FUSE for OS X 2.7.0 with MacFUSE compatibility layer installed

I can mount and unmount functional (read/write) HFS disk images from the shell, but definitely not using the graphical DiskImageMounter/DiskImages UI Agent/Disk Utility. These commands work for me:

$: hdiutil attach /path/to/disk/image  
$: mount -t fusefs_hfs -o nodev,noowners,nosuid /dev/disk# /specify/a/mount/point  
\# where disk# is the value that hdiutil reports  
\# this works fine without the -o options, but they are here for consistency with hdiutil
\# And I can safely unmount the volume with just:  
$: umount /mount/point/you/specified  

Note that the call to hdiutil also includes a call to mount, which fails, but the subsequent call from the shell succeeds.