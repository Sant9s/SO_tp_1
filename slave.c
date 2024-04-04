#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include "pipes.h"


int main(){
    
    char* path;
    char* start_command = "md5sum %s";
    char command[MAX_PATH_SIZE - 2 + strlen(start_command)];
    char md5[MAX_MD5_SIZE + 1];     // ojo con el offset

    int continue_reading = 1;

    while(continue_reading == 1){
        continue_reading = pipe_read(FD_READ, path);    // errors are handled by wrappers

        // fijarse que no se rompa aca
    }

    sprintf(command, start_command, path);                        // store "md5sum + path" in command

    FILE *fp = popen(command, "r");                     // r means read

    if(fp == NULL){
        perror("popen error");
        exit(EXIT_FAILURE);
    }

    fgets(md5, MAX_MD5_SIZE, fp);   

    md5[MAX_MD5_SIZE+1] = '\0';

    pclose(fp);

    pipe_write(FD_WRITE, md5);


}