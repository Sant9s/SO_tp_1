// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "utils.h"
#include "shared_memory.h"
#include "pipes.h"

sem_t *initialize_semaphore(const char *name, int value);
char *create_shared_memory(const char * sh_mem_name, int *shm_fd);
void read_shared_memory(sem_t *shm_mutex_sem, sem_t *switch_sem, char *shared_memory);

sem_t *initialize_semaphore(const char *name, int value) {
    sem_t *sem = sem_open(name, O_CREAT, 0666, value);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    return sem;
}


char *create_shared_memory(const char * sh_mem_name, int *shm_fd) {
    
    *shm_fd = shm_open(sh_mem_name, O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    if (*shm_fd == -1) {
        perror("shm_open");
        printf("No MD5 program active.\n");
        exit(EXIT_FAILURE);
    }
    char *shared_memory = mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ, MAP_SHARED, *shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    return shared_memory;
}


void read_shared_memory(sem_t *shm_mutex_sem, sem_t *switch_sem, char *shared_memory) {
    int info_length = 0;

    while (1) {
        sem_wait(switch_sem);
        
        sem_wait(shm_mutex_sem);
        while (shared_memory[info_length] != '\n' && shared_memory[info_length] != '\t') {
            int i = strlen(shared_memory + info_length) + 1;
            if (i > 1) {
                printf("%s\n", shared_memory + info_length);
            }
            info_length += i;
        }

        if (shared_memory[info_length] == '\t') {
            sem_post(shm_mutex_sem);
            break;
        }
        sem_post(shm_mutex_sem);
    }
}

int main(int argc, char * argv[]) {
    sem_unlink(SWITCH_SEM_NAME);
    sem_unlink(SHM_SEM_NAME);

    sem_t *shm_mutex_sem = initialize_semaphore(SHM_SEM_NAME, 1);
    sem_t *switch_sem = initialize_semaphore(SWITCH_SEM_NAME, 0);

    int shm_fd;
    
    char *shared_memory;
    char shm_name[MAX_PATH] = {0};
    
    if (argc > 1){
        shared_memory = create_shared_memory(argv[1], &shm_fd);
    }
    else {
        read_pipe(STDIN_FILENO, shm_name);
        
        if (shm_name[0] == '\0'){
            printf(PATH_LIMITATION_ERROR);
            exit(1);
        }
        shared_memory = create_shared_memory(shm_name, &shm_fd);
    }
    
    read_shared_memory(shm_mutex_sem, switch_sem, shared_memory);
    
    close(shm_fd);
    sem_close(shm_mutex_sem);
    sem_close(switch_sem);

    return 0;
}