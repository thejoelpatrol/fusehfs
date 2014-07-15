//
//  log.h
//  FuseHFS
//
//  Created by Joel Cretan on 7/9/14.
//
//

#ifndef FuseHFS_log_h
#define FuseHFS_log_h

int log_to_file();

void log_invoking_command(int argc, char *argv[]);

void log_fuse_call(struct fuse_args *args);

#endif
