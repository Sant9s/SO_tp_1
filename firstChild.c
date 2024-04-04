#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char const *argv[]) {
    char *s = "Hola mundo!!!\n";

    int write_result = write(1, s, strlen(s));
    if (write_result == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    return 0;
}