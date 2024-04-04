#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "pipes.h"


int read_pipe(int fd, char* buff) {
    int i = 0;
    char read_char[1] = {1};

    while (read_char[0] != 0 && i < strlen(buff)) {
        int read_result  = read(fd, read_char, 1);

        if (read_result == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        buff[i++] = read_char[0];
    }

    buff[i] = 0;

    return i;
}

int write_pipe(int fd, const char *buff) {

    int write_result = write(fd, buff, strlen(buff) + 1);

    if (write_result == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    return write_result;
}