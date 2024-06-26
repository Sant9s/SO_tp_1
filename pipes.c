// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "pipes.h"
#include "utils.h"


int read_pipe(int fd, char* buff) {
    int i = 0;
    char read_char[1] = {1};

    while (read_char[0] != 0 && i < MAX_PATH_SIZE) {
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