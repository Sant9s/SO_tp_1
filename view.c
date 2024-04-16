#include "pipes.h"
#include "pshm_ucase.h"
#include "utils.h"

void initialize_semaphore(sem_t **shm_mutex_sem);
int initialize_shared_memory(char **shm, char* shm_name);
void read_shared_memory(sem_t *sem1, int shm_fd);

 
int main(int argc, char *argv[]) {    

    // Shared Memory
    char *shm;
    int shm_fd = 0;
    char shm_name[SHM_NAME_SIZE];
    sem_t *shm_mutex_sem;

    if (argc > 1){    // Me pasaron por parametro la shm
        shm_fd = initialize_shared_memory(&shm, argv[1]);
    }
    else {            // Me pipearon la shm
        read(STDIN_FILENO, shm_name, strlen(SHARED_MEMORY_NAME));
        if (shm_name[0] == '\0'){
            perror("Error - Shared memory name");
            exit(1);
        }
        shm_fd = initialize_shared_memory(&shm, shm_name);
    }

    initialize_semaphore(&shm_mutex_sem);

    read_shared_memory(shm_mutex_sem, shm_fd);

    close(shm_fd);
    sem_close(shm_mutex_sem);

    return 0;
}

int initialize_shared_memory(char **shared_memory, char* shm_name) {
    int shared_memory_fd = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
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
    *shm_mutex_sem = sem_open(SHARED_MEMORY_SEM_NAME, 0);
    if (*shm_mutex_sem == SEM_FAILED) {
        perror("Semaphore was not initialized");
        exit(EXIT_FAILURE);
    }
}

void read_shared_memory(sem_t *shm_sem, int shm_fd) {
    fprintf(stdout, "N° -- Slave PID -- MD5 -- Filename\n");
    char buff[1] = {1};
    char result[BUFFER_SIZE];
    int i = 0;

    while (1) {
        i++;
        sem_wait(shm_sem);
        int j; 
        for (j = 0; read(shm_fd, buff, 1) > 0 && *buff != '\n'; j++) {
            result[j] = buff[0];
        }
        if (strcmp(result, END_OF_VIEW) == 0) break;
        result[j] = '\0';
        fprintf(stdout, "%d - %s\n", i, result);
    }
}