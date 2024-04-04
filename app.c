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

int create_n_pipes(int n, int array[][2]);
int create_n_slaves(int n, int slave_array[][2], pid_t slave_pids[], int read_or_write);

#define MAX_SLAVES 5
#define BUFFER_SIZE 100
#define PARENT_TO_SLAVE 0
#define SLAVE_TO_PARENT 1

typedef struct {
    char filename[256];
    char md5[33];
    int slave_id;
} Result;

int main(int argc, char *argv[]) {
    // Verificar que se proporcionen los argumentos esperados
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo1> <archivo2> ...\n", argv[0]);
        return 1;
    }

    // Inicializar variables y estructuras de datos necesarias
    int num_files = argc - 1;
    int num_slaves = MAX_SLAVES ; // De momento usamos 5, hay que calcular la cantidad de esclavos segun la cantidad de archivos
    int remaining_files = num_files % num_slaves;
    int parent_to_slave_pipe[num_slaves][2];
    int slave_to_parent_pipe[num_slaves][2];
    pid_t slave_pids[num_slaves];

    // Crear pipes para comunicarse con los esclavos
    create_n_pipes(num_slaves, parent_to_slave_pipe);
    create_n_pipes(num_slaves, slave_to_parent_pipe);

    // Crear esclavos
    create_n_slaves(num_slaves, parent_to_slave_pipe, slave_pids, PARENT_TO_SLAVE);
    create_n_slaves(num_slaves, slave_to_parent_pipe, slave_pids, SLAVE_TO_PARENT);

    // Distribuir archivos entre los esclavos
    int files_sent = 1;
    for (int i = 0; i < num_slaves; i++) {
        write(parent_to_slave_pipe[i][1], argv[files_sent], strlen(argv[files_sent]) + 1);
        files_sent++;
    }

    // Esperar a que todos los esclavos terminen
    for (int i = 0; i < num_slaves; i++) {
        waitpid(slave_pids[i], NULL, 0);
    }

    // Bucle principal: esperar los resultados de los esclavos y almacenarlos en el buffer compartido
    int files_processed = 0;
    fd_set read_fds; // Conjunto de descriptores de archivo para select()
    while (files_processed < num_files) {
        FD_ZERO(&read_fds);
        int max_fd = -1;
        for (int i = 0; i < num_slaves; i++) {
            FD_SET(slaves[i][0], &read_fds);
            if (slaves[i][0] > max_fd) {
                max_fd = slaves[i][0];
            }
        }

        // Esperar a que uno de los pipes tenga datos disponibles para lectura
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("Error en select");
            return 1;
        }

        // Leer los resultados de los esclavos y almacenarlos en el buffer compartido
        for (int i = 0; i < num_slaves; i++) {
            if (FD_ISSET(slaves[i][0], &read_fds)) {
                // Leer el resultado del esclavo
                Result result;
                if (read(slaves[i][0], &result, sizeof(Result)) != sizeof(Result)) {
                    perror("Error al leer el resultado del esclavo");
                    return 1;
                }


                files_processed++;

                // Si quedan archivos por enviar, enviar uno nuevo al esclavo
                if (files_sent < num_files) {
                    write(slaves[i][1], argv[files_sent + 1], strlen(argv[files_sent + 1]) + 1);
                    files_sent++;
                }
            }
        }
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

pid_t create_n_slaves(int n, int slave_array[][2], pid_t slave_pids[], int read_or_write) {
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Error -- Slave not created");
            return 1;
        }
    }
    return pid;
}

void create_pipes_and_children(int parent_to_child_pipe[CHILD_QTY][2], int child_to_parent_pipe[CHILD_QTY][2], pid_t child_pid[CHILD_QTY], FILE * resultado_file, int * shm_fd) {
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