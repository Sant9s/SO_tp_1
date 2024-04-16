#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "utils.h"
#include "pipes.h"
#include <string.h>


int main(){
    
    char process_info[RESULT_SIZE];              
    char path[MAX_PATH_SIZE];
    char* start_command = "md5sum %s";
    char command[MAX_PATH_SIZE - 2 + strlen(start_command)];
    char md5[MAX_MD5_SIZE + 1];  
    int ready = 1;

    while (ready > 0) {
        ready = read_pipe(STDIN_FILENO, path);
        if (ready == -1) {
            perror("pipe_read error");
            exit(EXIT_FAILURE);
        }
        
        sprintf(command, start_command, path); // store "md5sum + (path)" in command

        FILE *fp = popen(command, "r"); // r means read

        if(fp == NULL){
            fprintf(stderr, "popen error");
            exit(EXIT_FAILURE);
        }

        fgets(md5, MAX_MD5_SIZE, fp);   

        md5[strlen(md5)] = '\0';                // set last character to NULL

        pclose(fp);

        sprintf(process_info, "%d  %s", getpid(), md5);

        write_pipe(FD_WRITE, process_info);         
    }

    close(STDOUT_FILENO);
    
    exit(EXIT_SUCCESS);

    return 0;
}