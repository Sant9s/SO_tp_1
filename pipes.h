//pipes.h
#ifndef _PIPES
#define _PIPES

#include <sys/wait.h>
#include <sys/select.h>

int read_pipe(int fd, char *buff);
int write_pipe(int fd, const char *buff);

#endif