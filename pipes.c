// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "utils.h"
#include "pipes.h"

int write_pipe(int fd, const char *buff){
    return write(fd, buff, strlen(buff)+1);
}

int read_pipe(int fd, char *buff){
    int i=0;
    char last_charater_read[1];
    last_charater_read[0]=1;

    while(last_charater_read[0]!=0 && last_charater_read[0]!='\n' && read(fd,last_charater_read,1)>0){
        buff[i++]=last_charater_read[0];
    }
    buff[i]=0;

    return i;
}