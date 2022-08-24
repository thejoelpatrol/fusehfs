For anyone's future reference trying to debug `diskarbitrationd`, here are some tips. You'll need to disable SIP for some of this.

Launching apps is not possible while `diskarbitrationd` is halted. Don't halt for too long (>5min?) or something will break and you'll need to reboot.

To see any diskarbitrationd messages in console, you'll need to do this:
https://georgegarside.com/blog/macos/sierra-console-private/
But you will only see a few select lines in the `console` app, very frustrating. 

So you probably want to invoke `diskarbitrationd` with logging on its next run (kill -9 to make `launchctl` restart it):
`$ sudo launchctl debug system/com.apple.diskarbitrationd -- /usr/libexec/diskarbitrationd -d`
its full log is then written to `/var/log/diskarbitrationd.log`; 

Attach lldb to diskarbitrationd:
`$ sudo launchctl attach system/com.apple.diskarbitrationd`
Since you don't have debugging symbols, you'll want a good disassembly to show you the way. IDA is good :)

Find ASLR base of the process you are attached to in lldb:
`(lldb) image dump sections`

In my case, one problem so far was related to a path being wrong in [DAFileSystemProbe](https://opensource.apple.com/source/DiskArbitration/DiskArbitration-342.80.2/diskarbitrationd/DAFileSystem.c.auto.html), resulting in `ENOTSUP` (0x0000002D) being reported in [__DAProbeCallback](https://opensource.apple.com/source/DiskArbitration/DiskArbitration-342.80.2/diskarbitrationd/DAProbe.c.auto.html). A path in memory may be a `CFURL` or `CFURLRef`, which Apple calls an "[opaque type](https://developer.apple.com/documentation/corefoundation/cfurl-rd7)". Given a pointer to a `CFURL`, the string you're probably looking for containing the actual path can be found like so:
```
struct CFURL {
  long field1;
  long field2;
  long field3;
  struct some_other_type *field4;
  ...
};

struct some_other_type {
  long field1;
  long field2;
  char path[];
  ...
};
```

And finally, for anyone else coming from gdb, here's a nice lldb cheat sheet:
https://lldb.llvm.org/use/map.html
