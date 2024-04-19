// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <string.h>
#include "pipes.h"
#include "utils.h"
#include <ctype.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>


int create_n_pipes(int n, int array[][2]);
int create_n_slaves(int n, pid_t slave_pids[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]);
void set_pipe_environment(int n, int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]);
void initialize_semaphore(sem_t **shm_mutex_sem);
int initialize_shared_memory(char **shared_memory);
void set_fd(int num_slaves, int slave_to_parent_pipe[][2], int * max_fd, fd_set * readfds);
void slave_handler(int num_files, int num_slaves, const char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], int *files_sent, char result[][RESULT_SIZE],  FILE *resultado_file, int shm_fd, sem_t *shm_mutex_sem);
void initialize_slave_pids(int num_slaves, pid_t slave_pids[]);
void set_file_config(FILE** file);
int distribute_initial_files(int num_files, const char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], int num_slaves);
void close_unused_pipes(int parent_to_child_pipe[][2], int child_to_parent_pipe[][2], int my_index, int num_slaves);
void free_shared_memory(sem_t *shm_mutex_sem, int shm_fd);


int main(int argc, const char *argv[]) {

    // Verify that user input file path
    if (argc < 2) {
        fprintf(stderr, "How to use: %s <file1> <fil2> ...\n", argv[0]);
        return 1;
    }

    // Shared memory
    sem_t *shm_mutex_sem;
    char *shared_memory;

    initialize_semaphore(&shm_mutex_sem);
    int shm_fd = initialize_shared_memory(&shared_memory);
    fprintf(stdout, SHARED_MEMORY_NAME);
    fflush(stdout); 

    // Initialize variables
    int num_files = argc - 1;
    int num_slaves = num_files/10 + 1; 
    int parent_to_slave_pipe[num_slaves][2];
    int slave_to_parent_pipe[num_slaves][2];
    pid_t slave_pids[num_slaves];
    char results[num_files][RESULT_SIZE];
    FILE* result_file = NULL;
    int files_assigned;

    // Sleep dos segundos
    // sleep(2);

    initialize_slave_pids(num_slaves, slave_pids);
    set_file_config(&result_file);

    // Create pipes to comunicate with slaves
    create_n_pipes(num_slaves, parent_to_slave_pipe);
    create_n_pipes(num_slaves, slave_to_parent_pipe);

    // Create slaves
    create_n_slaves(num_slaves, slave_pids, parent_to_slave_pipe, slave_to_parent_pipe);

    // Assign files to slaves
    files_assigned = distribute_initial_files(num_files, argv, parent_to_slave_pipe, slave_to_parent_pipe, num_slaves);

    slave_handler(num_files, num_slaves, argv,parent_to_slave_pipe,slave_to_parent_pipe, &files_assigned, results, result_file, shm_fd, shm_mutex_sem);

    fclose(result_file);

    write(shm_fd, END_OF_VIEW, strlen(END_OF_VIEW));
    sem_post(shm_mutex_sem);

    // Wait for slaves to finish
    for (int i = 0; i < num_slaves; i++) {
        int w = waitpid(slave_pids[i], NULL, 0);
        if (w == -1) {
        perror("waitpid");
        exit(EXIT_FAILURE);
        }
    }
    
    free_shared_memory(shm_mutex_sem, shm_fd);

    return 0;
}

int create_n_pipes(int n, int fd_array[][2]) {
    for (int i = 0; i < n; i++) {
        if (pipe(fd_array[i]) == -1) {
            perror("Pipe creation error");
            return -1;
        }
    }
    return 0;
}

void initialize_slave_pids(int num_slaves, pid_t slave_pids[]){
    for (int i = 0; i < num_slaves; i++) {
        slave_pids[i] = 0;
    }
}

void set_file_config(FILE** file){
    *file = fopen("result.txt", "wr");
    if (*file == NULL) {
        perror("fopen(result.txt)");
        exit(EXIT_FAILURE);
    }
    fprintf(*file, "N° -- Slave PID -- MD5 -- Filename\n");
}

int create_n_slaves(int n, pid_t slave_pid[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]) {
    for (int i = 0; i < n; i++) {
        slave_pid[i] = fork();
        if (slave_pid[i] == -1) {
            perror("Error -- Slave not created");
            return -1;
        }else if (slave_pid[i] == 0) {
            close_unused_pipes(parent_to_slave_pipe, slave_to_parent_pipe, i, n);
            set_pipe_environment(n, parent_to_slave_pipe, slave_to_parent_pipe);
            char *args[] = {"./slave", NULL};
            execve(args[0], args, NULL);
            exit(EXIT_FAILURE);
        }
    }

    for(int i=0; i < n; i++){
        close(parent_to_slave_pipe[i][FD_READ]);
        close(slave_to_parent_pipe[i][FD_WRITE]);
    }
    return 0;
}

void set_pipe_environment(int n, int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]){
    for (int i = 0; i < n; i++) {
        close(parent_to_slave_pipe[i][FD_WRITE]);
        close(slave_to_parent_pipe[i][FD_READ]);
        
        dup2(parent_to_slave_pipe[i][FD_READ], STDIN_FILENO);
        close(parent_to_slave_pipe[i][FD_READ]);

        dup2(slave_to_parent_pipe[i][FD_WRITE], STDOUT_FILENO);
        close(slave_to_parent_pipe[i][FD_WRITE]);
    }
}

void close_unused_pipes(int parent_to_child_pipe[][2], int child_to_parent_pipe[][2], int my_index, int num_slaves){
    for(int i=0; i<num_slaves; i++){
        if(i!=my_index){
            close(parent_to_child_pipe[i][FD_READ]);
            close(child_to_parent_pipe[i][FD_READ]);
            close(parent_to_child_pipe[i][FD_WRITE]);
            close(child_to_parent_pipe[i][FD_WRITE]);
        }
    }
}

int distribute_initial_files(int num_files, const char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], int num_slaves) {
    int files_assigned = 1;
    int n = num_slaves <= num_files ? num_slaves : num_files; 
    
    for (int i = 0; i < n; i++) {
        write_pipe(parent_to_slave_pipe[i][FD_WRITE], argv[files_assigned++]);
    }
    
    return files_assigned;
}

int initialize_shared_memory(char **shared_memory) {
    int shared_memory_fd = shm_open(SHARED_MEMORY_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shared_memory_fd == -1) {
        perror("Shared memory file creation error");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shared_memory_fd, SHM_SIZE) == -1) {
        perror("Size of shared memory file error");
        exit(EXIT_FAILURE);
    }

    *shared_memory = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);
    if (*shared_memory == MAP_FAILED) {
        perror("Shared memory file mapping error");
        exit(EXIT_FAILURE);
    }

    return shared_memory_fd;
}

void initialize_semaphore(sem_t **shm_mutex_sem) {
    *shm_mutex_sem = sem_open(SHARED_MEMORY_SEM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);
    if (*shm_mutex_sem == SEM_FAILED) {
        perror("Semaphore was not initialized");
        exit(EXIT_FAILURE);
    }
}

void free_shared_memory(sem_t *shm_mutex_sem, int shm_fd){
    sem_close(shm_mutex_sem);
    sem_unlink(SHARED_MEMORY_SEM_NAME);
    shm_unlink(SHARED_MEMORY_NAME);
    close(shm_fd);
}

void set_fd(int num_slaves, int slave_to_parent_pipe[][2], int * max_fd, fd_set * readfds){
       for (int i = 0; i < num_slaves; i++) {
        if (slave_to_parent_pipe[i][FD_READ] > *max_fd) {
            *max_fd = slave_to_parent_pipe[i][FD_READ];
        }
        FD_SET(slave_to_parent_pipe[i][FD_READ], readfds);
    }
}

void slave_handler(int num_files, int num_slaves, const char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], 
int *files_assigned, char results[][RESULT_SIZE],  FILE *result_file, int shm_fd, sem_t *shm_mutex_sem) {
    
    fd_set readfds;
    int max_fd = -1;
    int current_file = 0;

    set_fd(num_slaves, slave_to_parent_pipe, &max_fd, &readfds);

    while (current_file < num_files) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;
        
        set_fd(num_slaves, slave_to_parent_pipe, &max_fd, &readfds);

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_slaves; i++) {
            if (FD_ISSET(slave_to_parent_pipe[i][FD_READ], &readfds)) {
                int bytes_read = read_pipe(slave_to_parent_pipe[i][FD_READ], results[i]);
                if (bytes_read < 0) {
                    perror("read");
                    exit(EXIT_FAILURE);
                } else if (bytes_read == 0) {
                    close(slave_to_parent_pipe[i][FD_READ]);
                    FD_CLR(slave_to_parent_pipe[i][FD_READ], &readfds);
                } else {
                    // Add result to result.txt
                    fprintf(result_file, "%d° - %s", current_file + 1, results[i]);
                    fflush(result_file);

                    // Add result to shm
                    write(shm_fd, results[i], strlen(results[i]));
                    sem_post(shm_mutex_sem);

                    // Assign next file to process to a slave
                    if (*files_assigned <= num_files) {
                        write_pipe(parent_to_slave_pipe[i][FD_WRITE], argv[(*files_assigned)++]);
                    }
                    current_file++;
                }
            }
        }
    }
    
    for(int i=0; i < num_slaves; i++){
        write_pipe(parent_to_slave_pipe[i][FD_WRITE], "\0");
        close(parent_to_slave_pipe[i][FD_WRITE]);
        close(slave_to_parent_pipe[i][FD_READ]);
    }
}