#include "utils.h"
#include "pipes.h"

#define OFFSET 3


int main(){
    char path[MAX_PATH] = {0};             // the path of the file is stored here
    char md5[MAX_MD5 + MAX_PATH + OFFSET + 1];
    char *md5_cmd = "md5sum %s";
    char command[MAX_PATH + strlen(md5_cmd)];
    int ready = 1;
    // char process_info[RESULT_SIZE];                 // the slave pid, mds5 and filename is stored here
    
   
    while (ready > 0) {
        ready = read_pipe(STDIN_FILENO, path);
        if (ready == -1) {
            perror("pipe_read error");
            exit(EXIT_FAILURE);
        }

        if (path[0] == 0) {
            // No more files to read, exit the loop
            break;
        }
        
        sprintf(command, md5_cmd, path); // store "md5sum + (path)" in command

        FILE *fp = popen(command, "r"); // r means read

        if(fp == NULL){
            perror("popen");
            exit(EXIT_FAILURE);
        }

        fgets(md5, MAX_MD5 + strlen(path) + OFFSET, fp);   

        md5[MAX_MD5 + strlen(path) + OFFSET + 1] = '\0';                // set last character to NULL

        pclose(fp);


        write_pipe(STDOUT_FILENO, md5); // write the md5 to the pipe  
    }

    close(STDOUT_FILENO);
    
    exit(EXIT_SUCCESS);


}