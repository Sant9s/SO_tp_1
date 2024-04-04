#ifndef _UTILS
#define _UTILS

#define FD_READ 0
#define FD_WRITE 1

#define MAX_PATH_SIZE 100
#define MAX_MD5_SIZE 40

#define BUFFER_SIZE 1000

typedef struct {
    char filename[MAX_PATH_SIZE];
    char md5[MAX_MD5_SIZE];
    int slave_id;
} Result;





#endif