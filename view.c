#include "utils.h"
#include "pipes.h"
#include "pshm_ucase.h"


sem_t *initialize_semaphore(const char *name, int value) {
    sem_t *sem = sem_open(name, O_CREAT, 0666, value);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    return sem;
}

char *create_shared_memory(const char *shm_name, int *shm_fd) {
    *shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (*shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    ftruncate(*shm_fd, sizeof(struct shmbuf));
    return mmap(NULL, sizeof(struct shmbuf), PROT_READ | PROT_WRITE, MAP_SHARED, *shm_fd, 0);

    // Configurar el tamaño del archivo de memoria compartida
    if (ftruncate(*shm_fd, sizeof(struct shmbuf)) == -1) {
        perror("Error al configurar el tamaño del archivo de memoria compartida");
        exit(EXIT_FAILURE);
    }

    // Mapear el archivo de memoria compartida
    char *shmp = mmap(NULL, sizeof(*shmp), PROT_READ | PROT_WRITE, MAP_SHARED, *shm_fd, 0);
    if (shmp == MAP_FAILED) {
        perror("Error al mapear el archivo de memoria compartida");
        exit(EXIT_FAILURE);
    }
    
    return shmp;
}