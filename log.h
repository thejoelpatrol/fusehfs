//
//  log.h
//  FuseHFS
//
//  Created by Joel Cretan on 10/10/16.
//
//

#ifndef _log_h
#define _log_h

#define DEBUG

int log_to_file();
void log_invoking_command(int argc, char *argv[]);
void log_fuse_call(struct fuse_args *args);

#endif /* _log_h */
