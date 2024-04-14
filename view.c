#include "pipes.h"
#include "pshm_ucase.h"
#include "utils.h"

sem_t *initialize_semaphore(const char *name, int value);
char *create_shared_memory(const char *shm_name, int *shm_fd);
void read_shared_memory(sem_t *sem1, char *shm);


int main(int argc, char *argv[]) {

     
    sem_unlink(SHARED_MEMORY_SEM_NAME);
    

    char shm_name[MAX_PATH_SIZE] = {0};
    char *shm;

    sem_t *shm_mutex_sem = initialize_semaphore(SHARED_MEMORY_SEM_NAME, 1);

   

    int shm_fd;

    if(argc != 1 && argc !=4){
        printf("Wrong number of arguments\n");                  // aca aclarar como se usa
        return 1;
    }

    if (argc == 4) {                                            // shm recibido through stdin
        shm = create_shared_memory(argv[1], &shm_fd);
    }   
    else {                                                       // shm recieved through a pipe
        read_pipe(STDIN_FILENO, shm_name);
        
        if(shm_name[0] == '\0'){
            // shm was neither sent by parameter nor piped, close program 
            // PODRIAMOS MANDAR ALGUN MENSAJES DE ERROR
            exit(1);
        }
        if(shm_name[0] == '\0'){
            fprintf(stderr, "%s", shm_name); 
            exit(1);
        }
          
        
        // it was piped and is now in shm_name
        shm = create_shared_memory(shm_name, &shm_fd);
                  
    }

    read_shared_memory(shm_mutex_sem, shm);

    close(shm_fd);
    sem_close(shm_mutex_sem);

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
    *shm_fd = shm_open(shm_name, O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    
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

void read_shared_memory(sem_t *shm_sem, char *shm) {
    int length = 0;

    while (1) {
        sem_wait(shm_sem);
        while(shm[length] != '\n' && shm[length] != '\0') {            // por alguna razon shm[0] == '\0' entonces tira el loop infinitamente
           
            int i = strlen(shm + length) + 1;
            if (i > 1) {
                printf("%s\n", shm + length);
            }
            length += i;
        }

        if (shm[length] == '\t') {
            sem_post(shm_sem);
            break;
        }
        sem_post(shm_sem);
    }
}