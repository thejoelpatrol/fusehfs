//
//  log.c
//  FuseHFS
//
//  Created by Joel Cretan on 7/8/14.
//
//  Licensed under GPLv2: https://www.gnu.org/licenses/gpl-2.0.html
//
#include "common.h"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <limits.h>

#include "log.h"

#define MAC_FIRST_USER 501
#define MEGABYTE 1 << 20
#define NUM_MEGS_LOG 10

int log_to_file() {
    char logpath[PATH_MAX];
    
    // you can't write to /Library any more, so use ~/Library instead
    char *home = getpwuid(MAC_FIRST_USER)->pw_dir;
    if (strlen(home) + strlen(LOGPATH) >= PATH_MAX)
        return -1;
    strncpy(logpath, home, sizeof(logpath));
    strcat(logpath, LOGPATH);
    
    // delete old log if larger than 10MB so it doesn't get out of control
    // if we can't...that's probably fine.
    struct stat st;
    int rc = stat(logpath, &st);
    if (rc == 0) {
        if (st.st_size > NUM_MEGS_LOG * MEGABYTE) {
            unlink(logpath);
        }
    }
    
    int log = open(logpath, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR);
    chown(logpath, MAC_FIRST_USER, -1); // it's inconvenient for root, the owner of this process, to own the log
    if (log < 0) {
        fprintf(stderr, "open errno: %d\n", errno);
        return log;
    }
    if (dup2(log, STDERR_FILENO) < 0)
        fprintf(stderr, "stderr dup2 errno: %d\n", errno);
    fflush(stderr);
    return log;
}

void log_invoking_command(char *filename, int argc, char *argv[]) {
    time_t curr_time = time(NULL);
    char *current_time = ctime(&curr_time);
    current_time[strlen(current_time) - 1] = 0; // remove the \n at the end of the string
    fprintf(stderr, "\n%s %s -- invoked with argv: ", filename, current_time);
    for (int i = 0; i < argc; i++)
        fprintf(stderr, "%s ", argv[i]);
    fprintf(stderr, "\n");
    fflush(stderr);
}
