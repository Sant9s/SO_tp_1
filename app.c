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
int create_n_slaves(int n, pid_t slave_pids[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]);
void set_pipe_environment(int n, int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]);
// void initialize_shared_memory(int shm_fd, char *shmpath, struct shmbuf);
int initialize_shared_memory(void ** mem_pointer);
void set_fd(int num_slaves, int slave_to_parent_pipe[][2], int * max_fd, fd_set * readfds);
void slave_handler(int num_files, int num_slaves, const char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], int *files_sent, char result[][RESULT_SIZE],  FILE *resultado_file);
void initialize_slave_pids(int num_slaves, pid_t slave_pids[]);
void set_file_config(FILE** file);
int distribute_initial_files(int num_files, const char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], int num_slaves);
void close_unused_pipes(int parent_to_child_pipe[][2], int child_to_parent_pipe[][2], int my_index, int num_slaves);
void uninitialize_shared_memory(void * shm_ptr, int shm_fd);


int main(int argc, const char *argv[]) {
    // Verify that user input file path
    if (argc < 2) {
        fprintf(stderr, "How to use: %s <file1> <fil2> ...\n", argv[0]);
        return 1;
    }

    // shared memory stuff
    shm_unlink(SHARED_MEMORY_NAME);         // unlink any possible semaphores and shared memory from otrer excecutions
    sem_t *shm_mutex_sem;
    sem_t *switch_sem;
    char *shared_memory;
    int *view_opened = 0;


    // Initialize variables
    int num_files = argc - 1;
    int num_slaves = num_files/10 + 1;
    int parent_to_slave_pipe[num_slaves][2];
    int slave_to_parent_pipe[num_slaves][2];
    pid_t slave_pids[num_slaves];
    // int files_sent = 0;                 // lo usaremos con shared memory
    char results[num_files][RESULT_SIZE];
    FILE* result_file = NULL;
    int files_assigned;
    void *shm_ptr;



    // int shm_fd = initialize_shared_memory(&shm_ptr);

    fprintf(stdout, "%s\n", SHARED_MEMORY_NAME);                // prints /shared_memory

    // initialize_shared_memory(&shm_fd, &shared_memory, &shm_mutex_sem, &switch_sem, &view_opened, &result_file);


    initialize_slave_pids(num_slaves, slave_pids);
    set_file_config(&result_file);

    // Create pipes to comunicate with slaves
    create_n_pipes(num_slaves, parent_to_slave_pipe);
    create_n_pipes(num_slaves, slave_to_parent_pipe);

    // Create slaves
    create_n_slaves(num_slaves, slave_pids, parent_to_slave_pipe, slave_to_parent_pipe);

    files_assigned = distribute_initial_files(num_files, argv, parent_to_slave_pipe, slave_to_parent_pipe, num_slaves);

    slave_handler(num_files, num_slaves, argv,parent_to_slave_pipe,slave_to_parent_pipe, &files_assigned, results, result_file);

    // uninitialize_shared_memory(shm_ptr, shm_fd);

    
    fclose(result_file);

    // Wait for slaves to finish
    for (int i = 0; i < num_slaves; i++) {
        int w = waitpid(slave_pids[i], NULL, 0);
        if (w == -1) {
        fprintf(stderr, "waitpid");
        exit(EXIT_FAILURE);
        }
    }
    

    return 0;
}

int create_n_pipes(int n, int fd_array[][2]) {
    for (int i = 0; i < n; i++) {
        if (pipe(fd_array[i]) == -1) {
            perror("Pipe creation error");
            fprintf(stderr, "Pipe creation error");
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
        fprintf(stderr, "fopen(result.txt)");
        exit(EXIT_FAILURE);
    }
    fprintf(*file, "N° -- Slave PID -- MD5 -- Filename\n");
}

int create_n_slaves(int n, pid_t slave_pid[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]) {
    for (int i = 0; i < n; i++) {
        slave_pid[i] = fork();
        if (slave_pid[i] == -1) {
            perror("Error -- Slave not created");
            fprintf(stderr, "Error -- Slave not created");
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
        close(parent_to_slave_pipe[i][0]);
        close(slave_to_parent_pipe[i][1]);
    }
    return 0;
}

void set_pipe_environment(int n, int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]){
    for (int i = 0; i < n; i++) {
        close(parent_to_slave_pipe[i][1]);
        close(slave_to_parent_pipe[i][0]);

        //close(shm_fd);
        
        dup2(parent_to_slave_pipe[i][0], STDIN_FILENO);
        close(parent_to_slave_pipe[i][0]);

        dup2(slave_to_parent_pipe[i][1], STDOUT_FILENO);
        close(slave_to_parent_pipe[i][1]);

        // close(parent_to_slave_pipe[i][1]);
        // close(slave_to_parent_pipe[i][0]);
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

// int distribute_initial_files(int num_files, const char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]) {
    
//     int total_files_to_process = num_files;

//     int files_assigned = 1;
//     for(int i=0; i < 2 && i*NUM_SLAVES < total_files_to_process; i++){
//         for(int child_index=0; child_index < NUM_SLAVES && i*NUM_SLAVES+child_index < total_files_to_process; child_index++){
//             write_pipe(parent_to_slave_pipe[child_index][1], argv[files_assigned++]);
//         }
//     }

    
//     return files_assigned;
// }

int distribute_initial_files(int num_files, const char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], int num_slaves) {
    int files_assigned = 1;
    if (num_slaves <= num_files) {
        for (int i = 0; i < num_slaves; i++) {
        write_pipe(parent_to_slave_pipe[i][1], argv[files_assigned++]);
        }
    } else {
        for (int i = 0; i < num_files; i++) {
            write_pipe(parent_to_slave_pipe[i][1], argv[files_assigned++]);
        }
    }
    
    return files_assigned;
}

int initialize_shared_memory(void ** mem_pointer){
    int shared_memory_fd = shm_open("./shared_memory", O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (shared_memory_fd == -1) {
        perror("Shared memory file creation error");
        exit(EXIT_FAILURE);
    }

    // Configurar el tamaño del archivo de memoria compartida
    if (ftruncate(shared_memory_fd, 1048576) == -1) {
        perror("Size of shared memory file error");
        exit(EXIT_FAILURE);
    }


    //maps the shared memory into the process's available address
    void * shm_ptr = mmap(NULL, 1048576, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);      // 1048576 = 1MB 
    if (shm_ptr == MAP_FAILED) {
        perror("Shared mem mapping failed");
        exit(1);
    }

    // clears shared memory
    memset(shm_ptr, 0, 1048576);

    (*mem_pointer) = shm_ptr;

    return shared_memory_fd;

}


void uninitialize_shared_memory(void * shm_ptr, int shm_fd){
    if(munmap(shm_ptr, SHM_SIZE) == -1){
        perror("munmap failed");
        exit(1);
    }

    close(shm_fd);
    shm_unlink(SHARED_MEMORY_NAME);
}

// void initialize_shared_memory(int shared_memory_fd, char *shmpath, struct shmbuf) {
    
//     shared_memory_fd = shm_open(shmpath, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
//     if (shared_memory_fd == -1) {
//         perror("Shared memory file creation error");
//         exit(EXIT_FAILURE);
//     }

//     // Configurar el tamaño del archivo de memoria compartida
//     if (ftruncate(shared_memory_fd, sizeof(struct shmbuf)) == -1) {
//         perror("Size of shared memory file error");
//         exit(EXIT_FAILURE);
//     }

//     // Mapear el archivo de memoria compartida
//     struct shmbuf *shmp = mmap(NULL, sizeof(*shmp), PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);
//     if (shmp == MAP_FAILED) {
//         perror("Shared memory file mapping error");
//         exit(EXIT_FAILURE);
//     }

//     sleep(2);

//     if (sem_open(&shmp->sem1, 1) == SEM_FAILED) {
//         perror("Semaphore initialization error");
//         exit(EXIT_FAILURE);
//     }
//     if (sem_open(&shmp->sem2, 0) == SEM_FAILED) {
//         perror("Semaphore initialization error");
//         exit(EXIT_FAILURE);
//     }

//     if (sem_wait(&shmp->sem1) == -1) {
//         perror("Semaphore waiting error");
//         exit(EXIT_FAILURE);
//     }
//         dup2(parent_to_slave_pipe[i][0], STDIN_FILENO);
//         close(parent_to_slave_pipe[i][0]);

//         dup2(slave_to_parent_pipe[i][1], STDOUT_FILENO);
//         close(slave_to_parent_pipe[i][1]);

//         close(parent_to_slave_pipe[i][1]);
//         close(slave_to_parent_pipe[i][0]);
//     for (int j = 0; j < shmp->cnt; j++) {
//         shmp->buf[j] = toupper((unsigned char)shmp->buf[j]);
//     }

//     if (sem_post(&shmp->sem2) == -1) {
//         perror("Freeing semaphore error");
//         fprintf(stderr, "Freeing semaphore error");

//         exit(EXIT_FAILURE);
//     }

//     shm_unlink(shmpath);

//     exit(EXIT_SUCCESS);
// }

void set_fd(int num_slaves, int slave_to_parent_pipe[][2], int * max_fd, fd_set * readfds){
       for (int i = 0; i < num_slaves; i++) {
        if (slave_to_parent_pipe[i][0] > *max_fd) {
            *max_fd = slave_to_parent_pipe[i][0];
        }
        FD_SET(slave_to_parent_pipe[i][0], readfds);
    }
}

void slave_handler(int num_files, int num_slaves, const char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], 
int *files_assigned, char results[][RESULT_SIZE],  FILE *result_file) {
    
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
            fprintf(stderr, "select");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_slaves; i++) {
            if (FD_ISSET(slave_to_parent_pipe[i][0], &readfds)) {
                int bytes_read = read_pipe(slave_to_parent_pipe[i][0], results[i]);
                if (bytes_read < 0) {
                    perror("read");
                    fprintf(stderr, "read");
                    exit(EXIT_FAILURE);
                } else if (bytes_read == 0) {
                    close(slave_to_parent_pipe[i][0]);
                    FD_CLR(slave_to_parent_pipe[i][0], &readfds);
                } else {
                    fprintf(result_file, "%d° - %s", current_file + 1, results[i]);
                    fflush(result_file);
                    if (*files_assigned <= num_files) {
                        write_pipe(parent_to_slave_pipe[i][1], argv[(*files_assigned)++]);
                    }
                    current_file++;
                }
            }
        }
    }
    for(int i=0; i < num_slaves; i++){
        write_pipe(parent_to_slave_pipe[i][1], "\0");
        close(parent_to_slave_pipe[i][1]);
        close(slave_to_parent_pipe[i][0]);
    }
}