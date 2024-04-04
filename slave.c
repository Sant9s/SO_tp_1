#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "utils.h"
#include "pipes.h"
#include <string.h>


int main(){
    Result res;
    char path[MAX_PATH_SIZE];
    char* start_command = "md5sum %s";
    char command[MAX_PATH_SIZE - 2 + strlen(start_command)];
    char md5[MAX_MD5_SIZE + 1];     // ojo con el offset

    if (read_pipe(FD_READ, path) == 0){
        exit(1);
    }
    
    sprintf(command, start_command, path); // store "md5sum + path" in command

    FILE *fp = popen(command, "r"); // r means read

    if(fp == NULL){
        perror("popen error");
        exit(EXIT_FAILURE);
    }

    fgets(md5, MAX_MD5_SIZE, fp);   

    md5[MAX_MD5_SIZE] = '\0';

    pclose(fp);

    sprintf(res.filename, path);
    sprintf(res.md5, md5);
    res.slave_id = getpid();

    write_pipe(FD_WRITE, md5);

    return 0;
}