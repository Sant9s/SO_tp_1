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

#define MAX_SLAVES 5
#define BUFFER_SIZE 100

typedef struct {
    char filename[256];
    char md5[33];
    int slave_id;
} Result;

int create_n_pipes(int n, int array[][2]);
int create_n_slaves(int n, pid_t slave_pids[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]);
void set_pipe_environment(int n, int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]);

int main(int argc, char *argv[]) {
    // Verificar que se proporcionen los argumentos esperados
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo1> <archivo2> ...\n", argv[0]);
        return 1;
    }

    // Inicializar variables y estructuras de datos necesarias
    int num_files = argc - 1;
    int num_slaves = num_files / 10 ; // De momento usamos 5, hay que calcular la cantidad de esclavos segun la cantidad de archivos
    int parent_to_slave_pipe[num_slaves][2];
    int slave_to_parent_pipe[num_slaves][2];
    pid_t slave_pids[num_slaves];

    // Crear pipes para comunicarse con los esclavos
    create_n_pipes(num_slaves, parent_to_slave_pipe);
    create_n_pipes(num_slaves, slave_to_parent_pipe);

    // Crear esclavos
    create_n_slaves(num_slaves, slave_pids, parent_to_slave_pipe, slave_to_parent_pipe);

    // Distribuir archivos entre los esclavos
    int files_sent = 1;
    for (int i = 0; i < num_slaves; i++) {
        write_pipe(parent_to_slave_pipe[i][1], argv[files_sent]);
        files_sent++;
    }

    // Esperar a que todos los esclavos terminen 
    for (int i = 0; i < num_slaves; i++) {
        waitpid(slave_pids[i], NULL, 0);
    }

    return 0;
}

int create_n_pipes(int n, int fd_array[][2]) {
    for (int i = 0; i < n; i++) {
        if (pipe(fd_array[i]) == -1) {
            perror("Error al crear el pipe");
            return -1;
        }
    }
    return 0;
}

int create_n_slaves(int n, pid_t slave_pid[], int parent_to_slave_pipe[][2], int slave_to_parent_pipe[][2]) {
    for (int i = 0; i < n; i++) {
        slave_pid[i] = fork();
        if (slave_pid[i] == -1) {
            perror("Error -- Slave not created");
            return -1;
        }else if (slave_pid[i] == 0) {
            set_pipe_environment(n, parent_to_slave_pipe, slave_to_parent_pipe);
            char *args[] = {"./slave.elf", NULL};
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
        
        dup2(parent_to_slave_pipe[i][0], STDIN_FILENO);
        close(parent_to_slave_pipe[i][0]);

        dup2(slave_to_parent_pipe[i][1], STDOUT_FILENO);
        close(slave_to_parent_pipe[i][1]);
    }
}