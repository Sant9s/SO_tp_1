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
#include "pshm_ucase.h"


int create_n_pipes(int n, int array[][2]);
int create_n_slaves(int n, pid_t slave_pids[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], int shm_fd);
void set_pipe_environment(int n, int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], int shm_fd);
void initialize_shared_memory(int shm_fd, char *shmpath, struct shmbuf);
void set_fd(int num_slaves, int slave_to_parent_pipe[][2], int * max_fd, fd_set * readfds);
void slave_handler(int num_files, int num_slaves, char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], int *files_sent, char result[][RESULT_SIZE],  FILE *resultado_file);

int main(int argc, char *argv[]) {
    // Verify that user input file path
    if (argc < 2) {
        fprintf(stderr, "How to use: %s <archivo1> <archivo2> ...\n", argv[0]);
        return 1;
    }

    int shm_fd = 0;
    // char *shmpath = argv[1];
    // struct shmbuf shmbuf;
    // initialize_shared_memory(shm_fd, shmpath, shmbuf);

    // Initialize variables
    int num_files = argc - 1;
    int num_slaves = 1;
    //int num_slaves = num_files /10 + 1;
    int parent_to_slave_pipe[num_slaves][2];
    int slave_to_parent_pipe[num_slaves][2];
    pid_t slave_pids[num_slaves];
    int files_sent = 0;
    char results[num_slaves][RESULT_SIZE];
    FILE* result_file = fopen("result.txt", "wr");
    sprintf(result_file, "Slave PID -- MD5 -- Filename");

    if (result_file == NULL) {
        perror("fopen(result.txt)");
        exit(EXIT_FAILURE);
    }

    // Create pipes to comunicate with slaves
    create_n_pipes(num_slaves, parent_to_slave_pipe);
    create_n_pipes(num_slaves, slave_to_parent_pipe);

    // Create slaves
    create_n_slaves(num_slaves, slave_pids, parent_to_slave_pipe, slave_to_parent_pipe, shm_fd);

    // Distribute inicial files between slaves
    for (int i = 0; i < num_slaves; i++) {
        write_pipe(parent_to_slave_pipe[i][1], argv[files_sent++ + 1]);
    }

    slave_handler(num_files,num_slaves, argv,parent_to_slave_pipe,slave_to_parent_pipe, &files_sent, results, result_file);

    // Wait for slaves to finish
    for (int i = 0; i < num_slaves; i++) {
        waitpid(slave_pids[i], NULL, 0);
    }

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

int create_n_slaves(int n, pid_t slave_pid[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], int shm_fd) {
    for (int i = 0; i < n; i++) {
        slave_pid[i] = fork();
        if (slave_pid[i] == -1) {
            perror("Error -- Slave not created");
            return -1;
        }else if (slave_pid[i] == 0) {
            set_pipe_environment(n, parent_to_slave_pipe, slave_to_parent_pipe, shm_fd);
            char *args[] = {"./slave", NULL};
            execve(args[0], args, NULL);
            exit(EXIT_FAILURE);
        }
    }

    for(int i=0; i < n; i++){
        close(parent_to_slave_pipe[i][0]);
        close(slave_to_parent_pipe[i][1]);
    }
    return 0;
}

void set_pipe_environment(int n, int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], int shm_fd){
    for (int i = 0; i < n; i++) {
        close(parent_to_slave_pipe[i][1]);
        close(slave_to_parent_pipe[i][0]);

        close(shm_fd);
        
        dup2(parent_to_slave_pipe[i][0], STDIN_FILENO);
        close(parent_to_slave_pipe[i][0]);

        dup2(slave_to_parent_pipe[i][1], STDOUT_FILENO);
        close(slave_to_parent_pipe[i][1]);
    }
}

void initialize_shared_memory(int shared_memory_fd, char *shmpath, struct shmbuf) {
    
    shared_memory_fd = shm_open(shmpath, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (shared_memory_fd == -1) {
        perror("Shared memory file creation error");
        exit(EXIT_FAILURE);
    }

    // Configurar el tamaño del archivo de memoria compartida
    if (ftruncate(shared_memory_fd, sizeof(struct shmbuf)) == -1) {
        perror("Size of shared memory file error");
        exit(EXIT_FAILURE);
    }

    // Mapear el archivo de memoria compartida
    struct shmbuf *shmp = mmap(NULL, sizeof(*shmp), PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);
    if (shmp == MAP_FAILED) {
        perror("Shared memory file mapping error");
        exit(EXIT_FAILURE);
    }

    sleep(2);

    if (sem_open(&shmp->sem1, 1) == SEM_FAILED) {
        perror("Semaphore initialization error");
        exit(EXIT_FAILURE);
    }
    if (sem_open(&shmp->sem2, 0) == SEM_FAILED) {
        perror("Semaphore initialization error");
        exit(EXIT_FAILURE);
    }

    if (sem_wait(&shmp->sem1) == -1) {
        perror("Semaphore waiting error");
        exit(EXIT_FAILURE);
    }

    for (int j = 0; j < shmp->cnt; j++) {
        shmp->buf[j] = toupper((unsigned char)shmp->buf[j]);
    }

    if (sem_post(&shmp->sem2) == -1) {
        perror("Freeing semaphore error");
        exit(EXIT_FAILURE);
    }

    shm_unlink(shmpath);

    exit(EXIT_SUCCESS);
};

void set_fd(int num_slaves, int slave_to_parent_pipe[][2], int * max_fd, fd_set * readfds){
       for (int i = 0; i < num_slaves; i++) {
        if (slave_to_parent_pipe[i][0] > *max_fd) {
            *max_fd = slave_to_parent_pipe[i][0];
        }
        FD_SET(slave_to_parent_pipe[i][0], readfds);
    }
}

void slave_handler(int num_files, int num_slaves, char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], 
int *files_sent, char results[][RESULT_SIZE],  FILE *result_file) {
    
    fd_set readfds;
    int max_fd = -1;
    int current_file = 0;

    set_fd(num_slaves, slave_to_parent_pipe, &max_fd, &readfds);

    while (current_file < num_files ) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;
        
        set_fd(num_slaves, slave_to_parent_pipe, &max_fd, &readfds);

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }


        for (int i = 0; i < num_slaves; i++) {
            if (FD_ISSET(slave_to_parent_pipe[i][0], &readfds)) {
                int bytes_read = read_pipe(slave_to_parent_pipe[i][0], results[i]);
                if (bytes_read < 0) {
                    perror("read");
                    exit(EXIT_FAILURE);
                } else if (bytes_read == 0) {
                    close(slave_to_parent_pipe[i][0]);
                    FD_CLR(slave_to_parent_pipe[i][0], &readfds);
                } else {
                    fprintf(result_file, "%s", results[i]);
                    fflush(result_file);
                    
                    if (*files_sent <= num_files) {
                        write_pipe(parent_to_slave_pipe[i][1], argv[(*files_sent)++ +1]);
                    }

                    current_file++;
                }
            }
        }
    }
    for(int i=0; i < num_slaves; i++){
        write_pipe(parent_to_slave_pipe[i][1], "\0");
    }
}