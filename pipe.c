#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main(int argc, char const *argv[]) {
    int p[2];

    if (pipe(p) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid_firstChild = fork();
    if (pid_firstChild < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid_firstChild == 0) {
        close(p[0]);
        dup2(p[1], 1);
        close(p[1]);
        char *args[] = {"firstChild", NULL};
        execve("./firstChild", args, NULL);

        perror("execve");
        exit(EXIT_FAILURE);
    }

    close(p[1]);

    int w1 = waitpid(pid_firstChild, NULL, 0);
    if (w1 == -1) {
      perror("waitpid");
      exit(EXIT_FAILURE);
    }


    char *newargv[] = {"secondChild", NULL};
    pid_t pid_secondChild = fork();
    if (pid_secondChild < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid_secondChild == 0) {
        close(p[1]);
        dup2(p[0], 0);
        close(p[0]);
        execve("./secondChild", newargv, NULL);

        perror("execve");
        exit(EXIT_FAILURE);
    }

    close(p[0]);

    int w2 = waitpid(pid_secondChild, NULL, 0);
    if (w2 == -1) {
      perror("waitpid");
      exit(EXIT_FAILURE);
    }

    return 0;
}