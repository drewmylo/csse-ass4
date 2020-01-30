
#ifndef ERROR_H
#define ERROR_H
#include <stdio.h>

typedef enum {
    NORMAL = 0,   // All is well
    BAD_ARGS = 1, // Usage: player players myid threshold handsize
    BAD_NAME = 2,
    BAD_QTY = 3
} Error;

const char *error_message(Error error);
Error exit_with_error(Error error);

#endif
