#ifndef UTILS_H
#define UTILS_H
#include "channel.h"
#include <stdio.h>
#include <pthread.h>

typedef enum {
    CONNECT = 1,
    IM = 2,
    DELIVER = 3,
    WITHDRAW = 4,
    TRANSFER = 5,
    EXECUTE = 6,
    NEIGHBOUR = 7,
    HUPPED = 8
} Message;

typedef struct {
    char *productName;
    int productQuantity;
} Product;

typedef struct {
    pid_t threadId;
    char *name;
    unsigned int port;
    FILE *writeTo;
    FILE *readFrom;
} Neighbour;

typedef struct {
    bool deferred;
    Message message;
    char *depotName;
    char *productName;
    int productQuantity;
    unsigned int key;
    unsigned int port;
    pid_t threadId;
    FILE *writeTo;
    FILE *readFrom;

} ReceivedOperation;

typedef struct {
    char *name;
    unsigned int port;
    Product *inventory;
    Neighbour *neighbours;
    ReceivedOperation *defferedOps;
    int defferedCount;
    int neighbourCount;
    int inventoryCount;
    struct Channel *channel;

} Depot;

typedef struct {
    int writeFd;
    int readFd;
    struct Channel *channel;
    Depot *depot;
} InitArgs;

char *read_line_from_array(char *string, int *size, char delimeter);
char *read_line_from_file(FILE *file, int *size, char delimeter);

#endif
