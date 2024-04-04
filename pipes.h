//pipes.h
#ifndef _PIPES
#define _PIPES

int pipe_read(int fd, char *buff);
int pipe_write(int fd, const char *buff);

#endif