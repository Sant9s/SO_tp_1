#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUF_SIZE 20

int readPipe(char *s);

int main(int argc, char const *argv[]) {
    char s[BUF_SIZE];


    readPipe(s);
    
    printf("%s\n", s);
	return 0;
}

int readPipe(char *s) {
    int i = 0;
    char r[1];
    r[0] = 1;

    while (r[0] != 0 && read(0, r, 1) > 0) {
        s[i++] = r[0];
    }

    s[i] = 0;

    return i;
}