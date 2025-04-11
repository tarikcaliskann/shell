#ifndef MODEL_H
#define MODEL_H

#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#define BUF_SIZE 4096
#define SHARED_FILE_PATH "mymsgbuf"
#define MAX_CMD_LENGTH 1024
#define MAX_OUTPUT_LENGTH 4096

typedef struct shmbuf {
    sem_t sem;
    size_t cnt;
    int fd;
    char msgbuf[];
} ShmBuf;

ShmBuf *buf_init();
void model_send_message(ShmBuf *shmp, const char *msg);
char *model_read_messages(ShmBuf *shmp);
char *execute_command(const char *cmd);
void cleanup(ShmBuf *shmp);

#endif
