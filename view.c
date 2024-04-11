#include "pipes.h"
#include "pshm_ucase.h"

sem_t *initialize_semaphore(const char *name, int value);
char *create_shared_memory(const char *shm_name, int *shm_fd);
void read_shared_memory(sem_t *sem1, sem_t *sem2, char *shm);


int main(int argc, char *argv[]) {
    sem_unlink("/sem1");
    sem_unlink("/sem2");

    char shm_name[100] = {0};
    char *shm;

    sem_t *sem1 = initialize_semaphore("/sem1", 0);
    sem_t *sem2 = initialize_semaphore("/sem2", 0);

    int shm_fd;

    if (argv > 1) {
        shm = create_shared_memory(argv[1], &shm_fd);
    }
    else {
        read_pipe(0, shm_name);
        shm = create_shared_memory(shm_name, &shm_fd);
    }

    read_shared_memory(sem1, sem2, shm);

    close(shm_fd);
    sem_close(sem1);
    sem_close(sem2);

    return 0;
}

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

void read_shared_memory(sem_t *sem1, sem_t *sem2, char *shm) {
    int length = 0;

    while (1) {
        sem_wait(sem2);
        sem_wait(sem1);
        while(shm[length] != '\n' && shm[length] != '\0') {
            int i = strlen(shm + length) + 1;
            if (i > 1) {
                printf("%s\n", shm + length);
            }
            length++;
        }

        if (shm[length] == '\t') {
            sem_post(sem1);
            break;
        }
        sem_post(sem1);
    }
}