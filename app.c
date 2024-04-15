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
// int initialize_shared_memory(void ** mem_pointer);
typedef int semaphore;
int initialize_shared_memory(char **shared_memory, sem_t **shm_mutex_sem);
void set_fd(int num_slaves, int slave_to_parent_pipe[][2], int * max_fd, fd_set * readfds);
void slave_handler(int num_files, int num_slaves, const char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], 
int *files_assigned, char results[][RESULT_SIZE],  FILE * result_file, char *shared_memory);
    
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

    // shared memory stuff
    shm_unlink(SHARED_MEMORY_NAME);         // unlink any possible semaphores and shared memory from otrer excecutions  
    sem_t *shm_mutex_sem;
    char *shared_memory;
    int shm_fd = initialize_shared_memory(&shared_memory, &shm_mutex_sem);


    // Initialize variables
    int num_files = argc - 1;
    int num_slaves = 3;
    int parent_to_slave_pipe[num_slaves][2];
    int slave_to_parent_pipe[num_slaves][2];
    pid_t slave_pids[num_slaves];
    // int files_sent = 0;                 // lo usaremos con shared memory
    char results[num_files][RESULT_SIZE];
    FILE* result_file = NULL;
    int files_assigned;



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

    slave_handler(num_files, num_slaves, argv,parent_to_slave_pipe,slave_to_parent_pipe, &files_assigned, results, result_file, shared_memory);

    free_shared_memory(shm_mutex_sem, shm_fd);

    
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

int initialize_shared_memory(char **shared_memory, sem_t **shm_mutex_sem){
    int shared_memory_fd = shm_open(SHARED_MEMORY_NAME, O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    
    if (shared_memory_fd == -1) {
        perror("Shared memory file creation error");
        exit(EXIT_FAILURE);
    }

    // Configurar el tamaño del archivo de memoria compartida
    if (ftruncate(shared_memory_fd, SHM_SIZE) == -1) {
        perror("Size of shared memory file error");
        exit(EXIT_FAILURE);
    }

        // Mapear el archivo de memoria compartida
    *shared_memory = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);
    if (*shared_memory == MAP_FAILED) {
        perror("Shared memory file mapping error");
        exit(EXIT_FAILURE);
    }

    *shm_mutex_sem = sem_open(SHARED_MEMORY_SEM_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1);
    if(*shm_mutex_sem == SEM_FAILED) {
        perror("Semaphore was not initialized");
        exit(EXIT_FAILURE);
    }

    sleep(2);
    

    return shared_memory_fd;


}

void free_shared_memory(sem_t *shm_mutex_sem, int shm_fd){
    sem_close(shm_mutex_sem);
    sem_unlink(SHARED_MEMORY_NAME);
    close(shm_fd);
    shm_unlink(SHARED_MEMORY_NAME);
}

void set_fd(int num_slaves, int slave_to_parent_pipe[][2], int * max_fd, fd_set * readfds){
       for (int i = 0; i < num_slaves; i++) {
        if (slave_to_parent_pipe[i][0] > *max_fd) {
            *max_fd = slave_to_parent_pipe[i][0];
        }
        FD_SET(slave_to_parent_pipe[i][0], readfds);
    }
}

void slave_handler(int num_files, int num_slaves, const char *argv[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2], 
int *files_assigned, char results[][RESULT_SIZE],  FILE * result_file, char *shared_memory) {
    
    fd_set readfds;
    int max_fd = -1;
    int current_file = 0;
    int info_length = strlen("PID: %d - %s") + MAX_MD5_SIZE + MAX_PATH_SIZE + 2;

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
            if (FD_ISSET(slave_to_parent_pipe[i][0], &readfds)) {
                int bytes_read = read_pipe(slave_to_parent_pipe[i][0], results[i]);
                if (bytes_read < 0) {
                    perror("read");
                    exit(EXIT_FAILURE);
                } else if (bytes_read == 0) {
                    close(slave_to_parent_pipe[i][0]);
                    FD_CLR(slave_to_parent_pipe[i][0], &readfds);
                } else {                                                                                // there are things to read
                    fprintf(result_file, "%d° - %s\n", current_file + 1, results[i]);                     // ej: 1°- 12736  a8f5f167f44f4964e6c998dee827110c  ./test/Heyy10.txt 
                    fflush(result_file);
                    snprintf(shared_memory + current_file * info_length, info_length, "%s", results[i]);        // esto guarda en shared_memory la informacion de cada md5

                    printf("in md5:%s\n", shared_memory + current_file * info_length);                          // verifica que lo que busco en shm esta
                    if (*files_assigned <= num_files) {
                        write_pipe(parent_to_slave_pipe[i][1], argv[(*files_assigned)++]);
                    }
                    current_file++;
                }
            }
        }
    }

    sprintf(shared_memory + current_file * RESULT_SIZE, "\n");      // le coloca \n al final del nombre del archivo
    for(int i=0; i < num_slaves; i++){
        write_pipe(parent_to_slave_pipe[i][1], "\0");
        close(parent_to_slave_pipe[i][1]);
        close(slave_to_parent_pipe[i][0]);
    }
}