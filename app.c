// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "utils.h"
#include "pipes.h"
#include "shared_memory.h"

#define CHILD_QTY 5
#define INITIAL_FILES_PER_CHILD 2
#define INFO_TEXT "PID:%d - %s"

void check_paths_limitation(int argc, const char * argv[]);
void initialize_resources(int *shm_fd, char **shared_memory, sem_t **shm_mutex_sem, sem_t **switch_sem, int *vision_opened, FILE **resultado_file);
void create_pipes_and_children(int parent_to_child_pipe[CHILD_QTY][2], int child_to_parent_pipe[CHILD_QTY][2], pid_t child_pid[CHILD_QTY], FILE * resultado_file, int * shm_fd);
void handle_select_and_pipes(int argc, const char *argv[], int parent_to_child_pipe[CHILD_QTY][2], int child_to_parent_pipe[CHILD_QTY][2], int *files_assigned, pid_t child_pid[CHILD_QTY], char child_md5[CHILD_QTY][MAX_MD5 + MAX_PATH + 4], sem_t *shm_mutex_sem, sem_t *switch_sem, char *shared_memory, FILE *resultado_file, int *vision_opened);
void cleanup(FILE * resultado_file, sem_t *shm_mutex_sem, sem_t *switch_sem, int shm_fd, pid_t child_pid[CHILD_QTY], int parent_to_child_pipe[CHILD_QTY][2], int child_to_parent_pipe[CHILD_QTY][2]);
int distribute_initial_files(int argc, const char *argv[], int parent_to_child_pipe[][2], int child_to_parent_pipe[][2]);
void close_pipes_that_are_not_mine(int parent_to_child_pipe[][2], int child_to_parent_pipe[][2], int my_index);

int main(int argc, const char *argv[]) {
    check_paths_limitation(argc, argv);
    int vision_opened = 0;
    int shm_fd;
    char *shared_memory;
    if (!isatty(STDOUT_FILENO)) {
        write_pipe(STDOUT_FILENO, SHARED_MEMORY_NAME);
    } else {
        printf("%s\n", SHARED_MEMORY_NAME);
    }
    sem_t *shm_mutex_sem;
    sem_t *switch_sem;
    FILE *resultado_file;
    pid_t child_pid[CHILD_QTY];
    int parent_to_child_pipe[CHILD_QTY][2];
    int child_to_parent_pipe[CHILD_QTY][2];
    char child_md5[CHILD_QTY][MAX_MD5 + MAX_PATH + 4];
    int files_assigned;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    initialize_resources(&shm_fd, &shared_memory, &shm_mutex_sem, &switch_sem, &vision_opened, &resultado_file);
    create_pipes_and_children(parent_to_child_pipe, child_to_parent_pipe, child_pid, resultado_file, &shm_fd);
    files_assigned = distribute_initial_files(argc, argv, parent_to_child_pipe, child_to_parent_pipe);
    handle_select_and_pipes(argc, argv, parent_to_child_pipe, child_to_parent_pipe, &files_assigned, child_pid, child_md5, shm_mutex_sem, switch_sem, shared_memory, resultado_file, &vision_opened);
    cleanup(resultado_file, shm_mutex_sem, switch_sem, shm_fd, child_pid, parent_to_child_pipe, child_to_parent_pipe);

    return 0;
}

void check_paths_limitation(int argc, const char * argv[]){
    for (int i = 1; i < argc; i++ ){
        if (strlen(argv[i]) > MAX_PATH){
            if (!isatty(STDOUT_FILENO)) {
                write_pipe(STDOUT_FILENO, "\0");
            } else {
                printf(PATH_LIMITATION_ERROR);
            }
            exit(EXIT_FAILURE);
        }
    }
}

void initialize_resources(int *shm_fd, char **shared_memory, sem_t **shm_mutex_sem, sem_t **switch_sem, int *vision_opened, FILE **resultado_file) {
    
    *shm_fd = shm_open(SHARED_MEMORY_NAME, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    if (*shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    *shared_memory = mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, *shm_fd, 0);
    if (*shared_memory == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    
    sleep(2);

    *shm_mutex_sem = sem_open(SHM_SEM_NAME, 1);
    if (*shm_mutex_sem != SEM_FAILED) {
        (*vision_opened)++;
    }

    *switch_sem = sem_open(SWITCH_SEM_NAME, 0);
    if (*switch_sem != SEM_FAILED) {
        (*vision_opened)++;
    }

    if (*vision_opened == 1) {
        perror("missing 1 semaphore");
        exit(EXIT_FAILURE);
    } else if (*vision_opened == 0) {
        printf("No view detected - No semaphore initialization\n");
    }

    if (ftruncate(*shm_fd, SHARED_MEMORY_SIZE) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    *resultado_file = fopen("resultado.txt", "wr");
    if (*resultado_file == NULL) {
        perror("fopen(resultado.txt)");
        exit(EXIT_FAILURE);
    }
    
}

void close_pipes_that_are_not_mine(int parent_to_child_pipe[][2], int child_to_parent_pipe[][2], int my_index){
    for(int i=0; i<CHILD_QTY; i++){
        if(i!=my_index){
            for(int j=0; j<2; j++){
                close(parent_to_child_pipe[i][j]);
                close(child_to_parent_pipe[i][j]);
            }
        }
    }
}

void create_pipes_and_children(int parent_to_child_pipe[CHILD_QTY][2], int child_to_parent_pipe[CHILD_QTY][2], pid_t child_pid[CHILD_QTY], FILE * resultado_file, int * shm_fd) {
    for (int i = 0; i < CHILD_QTY; i++) {
        if (pipe(parent_to_child_pipe[i]) == -1 || pipe(child_to_parent_pipe[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for(int i=0; i<CHILD_QTY; i++){
        child_pid[i] = fork();

        if (child_pid[i] == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (child_pid[i] == 0) {
            close_pipes_that_are_not_mine(parent_to_child_pipe, child_to_parent_pipe, i);

            close(parent_to_child_pipe[i][1]);
            close(child_to_parent_pipe[i][0]);
            fclose(resultado_file);
            close(*shm_fd);
            
            dup2(parent_to_child_pipe[i][0], STDIN_FILENO);
            close(parent_to_child_pipe[i][0]);

            dup2(child_to_parent_pipe[i][1], STDOUT_FILENO);
            close(child_to_parent_pipe[i][1]);

            char *args[] = {"./slave.elf", NULL};
            execve(args[0], args, NULL);
            exit(EXIT_FAILURE);
        }
    }

    for(int i=0; i<CHILD_QTY; i++){
        close(parent_to_child_pipe[i][0]);
        close(child_to_parent_pipe[i][1]);
    }

}

void set_fd(int child_to_parent_pipe[][2], int * max_fd, fd_set * readfds){
       for (int i = 0; i < CHILD_QTY; i++) {
        if (child_to_parent_pipe[i][0] > *max_fd) {
            *max_fd = child_to_parent_pipe[i][0];
        }
        FD_SET(child_to_parent_pipe[i][0], readfds);
    }
}

void handle_select_and_pipes(int argc, const char *argv[], int parent_to_child_pipe[CHILD_QTY][2], int child_to_parent_pipe[CHILD_QTY][2], 
int *files_assigned, pid_t child_pid[CHILD_QTY], char child_md5[CHILD_QTY][MAX_MD5 + MAX_PATH + 4], sem_t *shm_mutex_sem, sem_t *switch_sem, 
char *shared_memory, FILE *resultado_file, int *vision_opened) {
    fd_set readfds;
    int max_fd = -1;
    int current_file_index = 0;
    int total_files_to_process=argc-1;
    int info_length = strlen(INFO_TEXT) + MAX_MD5 + MAX_PATH + 2;

    set_fd(child_to_parent_pipe, &max_fd, &readfds);

    while (current_file_index < total_files_to_process) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;
        
        set_fd(child_to_parent_pipe, &max_fd, &readfds);

        int ready = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (ready == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        if (*vision_opened == 2) {
            sem_wait(shm_mutex_sem);
        }

        for (int i = 0; i < CHILD_QTY; i++) {
            if (FD_ISSET(child_to_parent_pipe[i][0], &readfds)) {
                int bytes_read = read_pipe(child_to_parent_pipe[i][0], child_md5[i]);
                if (bytes_read < 0) {
                    perror("read");
                    exit(EXIT_FAILURE);
                } else if (bytes_read == 0) {
                    close(child_to_parent_pipe[i][0]);
                    FD_CLR(child_to_parent_pipe[i][0], &readfds);
                } else {
                    fprintf(resultado_file, INFO_TEXT "\n", child_pid[i], child_md5[i]);
                    fflush(resultado_file);
                    
                    snprintf(shared_memory + current_file_index * info_length, info_length, INFO_TEXT, child_pid[i], child_md5[i]);
                    printf("in md5:%s\n", shared_memory + current_file_index * info_length);
                    if (*files_assigned < argc) {
                        write_pipe(parent_to_child_pipe[i][1], argv[(*files_assigned)++]);
                    }

                    current_file_index++;
                }
            }
        }
        
        sprintf(shared_memory + current_file_index * info_length, "\n");
        if (*vision_opened == 2) {
            sem_post(shm_mutex_sem);
            sem_post(switch_sem);
        }
    }
    for(int i=0; i<CHILD_QTY; i++){
        write_pipe(parent_to_child_pipe[i][1], "\0");
    }
    if (*vision_opened == 2) {
        sem_wait(shm_mutex_sem);
    }
    sprintf(shared_memory + current_file_index * info_length, "\t");
    if (*vision_opened == 2) {
        sem_post(shm_mutex_sem);
        sem_post(switch_sem);
    }
}

void cleanup(FILE * resultado_file, sem_t *shm_mutex_sem, sem_t *switch_sem, int shm_fd, pid_t child_pid[CHILD_QTY], int parent_to_child_pipe[CHILD_QTY][2], int child_to_parent_pipe[CHILD_QTY][2]) {
    for (int i = 0; i < CHILD_QTY; i++) {
        close(parent_to_child_pipe[i][1]);
        close(child_to_parent_pipe[i][0]);
    }

    for(int i=0; i<CHILD_QTY; i++){
        printf("\n\n\n*waiting%d*\n\n\n", i);
        waitpid(child_pid[i], NULL, 0);
    }
    
    sem_close(shm_mutex_sem);
    sem_unlink(SHM_SEM_NAME);

    sem_close(switch_sem);
    sem_unlink(SWITCH_SEM_NAME);

    fclose(resultado_file);

    close(shm_fd);
    shm_unlink(SHARED_MEMORY_NAME);
}

int distribute_initial_files(int argc, const char *argv[], int parent_to_child_pipe[][2], int child_to_parent_pipe[][2]) {
    
    int total_files_to_process=argc-1;

    int files_assigned = 1;
    for(int i=0; i<INITIAL_FILES_PER_CHILD && i*CHILD_QTY<total_files_to_process; i++){
        for(int child_index=0; child_index<CHILD_QTY && i*CHILD_QTY+child_index<total_files_to_process; child_index++){
            write_pipe(parent_to_child_pipe[child_index][1], argv[files_assigned++]);
        }
    }

    
    return files_assigned;
}