//
//  log.c
//  FuseHFS
//
//  Created by Joel Cretan on 7/8/14.
//
//

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <time.h>
#include <pwd.h>
#include <limits.h>

#define LOGPATH "/Library/Logs/fusehfs.log"

int log_to_file() {
    char logpath[PATH_MAX];
    char *home = getpwuid(getuid())->pw_dir;
    if (strlen(home) + strlen(LOGPATH) > PATH_MAX)
        return -1;
    strcpy(logpath, home);
    strcat(logpath, LOGPATH);
    int log = open(logpath, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR);
    if (log < 0) {
        fprintf(stderr, "open errno: %d\n", errno);
        return log;
    }
    if (dup2(log, STDOUT_FILENO) < 0)
        fprintf(stderr, "stdout dup2 errno: %d\n", errno);
    if (dup2(log, STDERR_FILENO) < 0)
        fprintf(stderr, "stderr dup2 errno: %d\n", errno);
    
    fflush(stderr);
    return log;
}

void log_invoking_command(int argc, char *argv[]) {
    time_t curr_time = time(NULL);
    char *current_time = ctime(&curr_time);
    current_time[strlen(current_time) - 1] = 0; // remove the \n at the end of the string
    printf("%s -- invoked with argv: ", current_time);
    for (int i = 0; i < argc; i++)
        printf("%s ", argv[i]);
    printf("\n");
    fflush(stdout);
}

