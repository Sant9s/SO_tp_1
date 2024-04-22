// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pipes.h"
#include "utils.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void initialize_semaphore(sem_t **shm_mutex_sem);
int initialize_shared_memory(char **shm, char* shm_name);
void read_shared_memory(int named_pipe_fd);

 
int main(int argc, char *argv[]) {   

    int named_pipe_fd = open(NAMED_PIPE, O_RDONLY);

    read_shared_memory(named_pipe_fd);

    close(named_pipe_fd);

    return 0;
}


void read_shared_memory(int named_pipe_fd){
    fprintf(stdout, "N° -- Slave PID -- MD5 -- Filename\n");
    char buff[1] = {1};
    char result[BUFFER_SIZE];
    int i = 0;

    while (1) {
        i++;
        int j; 
        for (j = 0; read(named_pipe_fd, buff, 1) > 0 && *buff != '\n'; j++) {
            result[j] = buff[0];
        }
        if (strcmp(result, END_OF_VIEW) == 0) break; 
        result[j] = '\0';
        fprintf(stdout, "%d° - %s\n", i, result);
    }
}