//
//  log.h
//  FuseHFS
//
//  Created by Joel Cretan on 10/10/16.
//
//

#ifndef _log_h
#define _log_h

#include "common.h"

#define LOGPATH "/Library/Logs/fusehfs.log"

int log_to_file();
void log_invoking_command(char *filename, int argc, char *argv[]);

#endif /* _log_h */
