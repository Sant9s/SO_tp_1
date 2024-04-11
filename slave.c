#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "utils.h"
#include "pipes.h"
#include <string.h>


int main(){
    char res[RESULT_SIZE] = "%s - %s - %d\n"; // "Filename - MD5 - Slave ID"
    char path[MAX_PATH_SIZE];
    char* start_command = "md5sum %s";
    char command[MAX_PATH_SIZE - 2 + strlen(start_command)];
    char md5[MAX_MD5_SIZE + 1];     // ojo con el offset

    if (read_pipe(FD_READ, path) == 0){
        fprintf(stderr, "pipe error");
        exit(1);
    }
    
    sprintf(command, start_command, path); // store "md5sum + path" in command

    FILE *fp = popen(command, "r"); // r means read

    if(fp == NULL){
        fprintf(stderr, "popen error");
        exit(EXIT_FAILURE);
    }

    fgets(md5, MAX_MD5_SIZE, fp);   

    md5[strlen(md5)] = '\0'; // seteo el ultimo caracter a 0 

    pclose(fp);

    fprintf(stderr, "%s\n", path);
    fprintf(stderr, "%s\n", md5);
    fprintf(stderr, "%d\n", getpid());

    sprintf(res, path, md5, getpid());

    write_pipe(FD_WRITE, res);

    close(STDOUT_FILENO);
    
    exit(EXIT_SUCCESS);

    return 0;
}