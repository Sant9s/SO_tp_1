#ifndef _UTILS
#include <string.h>
#define _UTILS

#define FD_READ 0
#define FD_WRITE 1

#define MAX_PATH_SIZE 100
#define MAX_MD5_SIZE 100
#define SHM_SIZE 1048576                        // 1MB
#define SHM_NAME_SIZE 20
#define BUFFER_SIZE 1000
#define RESULT_SIZE 20 + MAX_PATH_SIZE + MAX_MD5_SIZE

#define NUM_SLAVES 5
#define INITIAL_FILES_PER_SLAVE 2

#define SHARED_MEMORY_NAME "/shared_memory"
#define SHARED_MEMORY_SEM_NAME "/shm_sem"
#define HASH_SEM_NAME "/hash_sem"
#define END_OF_VIEW "THIS_IS_THE_END"


#endif