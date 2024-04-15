#ifndef _SHARED_MEMORY
#define _SHARED_MEMORY


#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define SHARED_MEMORY_NAME "/my_shared_memory"
#define SHM_SEM_NAME "/shm_mutex_sem"
#define SWITCH_SEM_NAME "/switch_sem"

#define SHARED_MEMORY_SIZE 1048576 // 1MB
#define PATH_LIMITATION_ERROR "PATH LENGTH EXCEEDS LIMIT - TERMINATING\n"

#endif