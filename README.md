fusehfs
=======

Goal: update FuseHFS (http://namedfork.net/fusehfs) to work with FUSE for OS X

This FUSE module was designed for MacFUSE, which is no longer maintained and doesn't work on newer versions of OS X. The FUSE for OS X (https://osxfuse.github.io) project picked up the FUSE-on-Mac baton, so that's the way to go from now on. Apple is nothing if not willing to drop support for outdated technologies, so Macs running OS X 10.6 onward can't write to HFS volumes. Evening reading seems to be broken as of 10.9, if not earlier.

I can't promise that I'll be able to get this working. I have a personal interest in restoring this functionality, and a very basic understanding of filesystems. I'm hoping to learn a lot through this project, so we'll see how it goes.

The original fusehfs code was published under GPL v2, so this version's code is too.
