#include <stdlib.h>
#include "utils.h"
#include "pipes.h"


int main(){
    
    // necesito recibir el path por un pipe
    char* path;
    char* md5 = "md5sum %s";

    // aca recibo el path por un pipe

    int continue_reading = 1;

    while(continue_reading == 1){
        continue_reading = pipe_read(FD_READ, path);    // errors are handled by wrappers
        
    }




}