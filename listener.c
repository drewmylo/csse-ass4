#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include "depot.h"
#include "error.h"
#include "utils.h"
#include "channel.h"

#define BUFFSIZE 256

/**
 *
 * @param  {char*} name : name to check
 * @param  {int} size   : size of name
 * @return {bool}       : true if ok, false otherwise
 */
bool check_names(char *name, int size)
{
    for (int i = 0; i < size; i++) {
        if (name[i] == ' ' || name[i] == '\r' || name[i] == ':'
                || name[i] == '\n') {
            return false;
        }
    }
    return true;
}

/**
 *
 * @param  {char*} message              : message to parse
 * @param  {ReceivedOperation*} product : product struct to parse into
 * @param  {bool} transfer              : transfer flag
 * @return {int}                        : return status, 0 means OK
 */
int parse_quant_type(char *message, ReceivedOperation *product, bool transfer)
{

    int size = 0;
    char *err1;

    product->productQuantity =
            strtoul(read_line_from_array(message, &size, ':'), &err1, 10);

    if (*err1 != '\0' || product->productQuantity < 1) {
        return 1;
    }

    message += size - 1;

    if (transfer) {
        product->productName = read_line_from_array(message, &size, ':');
    } else {
        product->productName = read_line_from_array(message, &size, '\n');

        if (message[(size) - 2] != '\n')
        {
            return 1;
        }
        
    }

    if (!check_names(product->productName, size) || size < 1) {
        return 1;
    }

    if (transfer) {
        message += size - 1;
        product->depotName = read_line_from_array(message, &size, '\n');
        if (size < 1 || message[(size) - 2] != '\n') {
            return 1;
        }
        if (!check_names(product->depotName, size)) {
            return 1;
        }


    }
    return 0;
}

/**
 * ReceivedOperation*parse_execute
 *
 * @param  {char*} message              : message to parse
 * @return {ReceivedOperation*}
 *     : Allocated and filled ReceivedOperation struct
 */
ReceivedOperation *parse_execute(char *message)
{
    ReceivedOperation *operation = calloc(1, sizeof(ReceivedOperation));
    if (strncmp(message, "Execute:", 8) != 0) {
        return NULL;
    }
    message += strlen("Execute:");

    char *err;
    int size = 0;
    unsigned int result
            = strtoul(read_line_from_array(message, &size, ':'), &err, 10);
    if (size < 1 || message[(size) - 2] != '\n') {
        return NULL;
    }
    if (*err != '\0' || result < 0) {
        return NULL;
    } else {
        operation->key = result;
        operation->message = EXECUTE;
        return operation;
    }
}

/**
 * ReceivedOperation*parse_delivery
 *
 * @param  {char*} message              : message to parse
 * @return {ReceivedOperation*}
 *     : Allocated and filled ReceivedOperation struct
 */
ReceivedOperation *parse_delivery(char *message)
{
    ReceivedOperation *operation = calloc(1, sizeof(ReceivedOperation));
    if (strncmp(message, "Deliver:", 8) != 0) {
        return NULL;
    }
    message += strlen("Deliver:");
    if (parse_quant_type(message, operation, false) == 1) {
        return NULL;
    } //check return value here todo
    operation->message = DELIVER;
    return operation;
}

/**
 * ReceivedOperation*parse_withdrawral
 *
 * @param  {char*} message              : message to parse
 * @return {ReceivedOperation*}
 *     : Allocated and filled ReceivedOperation struct
 */
ReceivedOperation *parse_withdrawral(char *message)
{

    if (strncmp(message, "Withdraw:", 9) != 0) {
        return NULL;
    }
    ReceivedOperation *operation = calloc(1, sizeof(ReceivedOperation));
    message += strlen("Withdraw:");
    if (parse_quant_type(message, operation, false) == 1) {
        return NULL;
    } //check return value here todo
    operation->message = WITHDRAW;
    return operation;
}

/**
 * ReceivedOperation*parse_transfer
 *
 * @param  {char*} message              : message to parse
 * @return {ReceivedOperation*}
 *      : Allocated and filled ReceivedOperation struct
 */
ReceivedOperation *parse_transfer(char *message)
{
    if (strncmp(message, "Transfer:", 9) != 0) {
        return NULL;
    }
    ReceivedOperation *operation = calloc(1, sizeof(ReceivedOperation));
    message += strlen("Transfer:");
    if (parse_quant_type(message, operation, true) == 1) {
        return NULL;
    } //check return value here todo
    operation->message = TRANSFER;
    return operation;
}

/**
 * ReceivedOperation*parse_deferred
 *
 * @param  {char*} message              : message to parse
 * @return {ReceivedOperation*}
 *     : Allocated and filled ReceivedOperation struct
 */
ReceivedOperation *parse_deferred(char *message) {
    ReceivedOperation *operation = calloc(1, sizeof(ReceivedOperation));
    if (strncmp(message, "Defer:", 6) != 0) {
        return NULL;
    }
    message += strlen("Defer:");
    int size = 0;
    char *err;
    unsigned int key
            = strtoul(read_line_from_array(message, &size, ':'), &err, 10);
    if (*err != '\0' || key < 0) {
        return NULL;
    }

    message += (size - 1);
    switch (message[0]) {
        case 'D':
            operation = parse_delivery(message);
            if (operation != NULL) {
                operation->message = DELIVER;
                operation->deferred = true;
                operation->key = key;
            }
            return operation;
            break;

        case 'W':
            operation = parse_withdrawral(message);
            if (operation != NULL) {
                operation->message = WITHDRAW;
                operation->deferred = true;
                operation->key = key;
            }
            return operation;
            break;

        case 'T':
            operation = parse_transfer(message);
            if (operation != NULL) {
                operation->message = TRANSFER;
                operation->deferred = true;
                operation->key = key;
            }
            return operation;
            break;
        default:
            return NULL;
            break;
    }
    return NULL;
}

/**
 * ReceivedOperation*parse_im
 *
 * @param  {char*} message              : message to parse
 * @return {ReceivedOperation*}
 *     : Allocated and filled ReceivedOperation struct
 */
ReceivedOperation *parse_im(char *message)
{
    ReceivedOperation *operation = calloc(1, sizeof(ReceivedOperation));
    if (strncmp(message, "IM:", 3) != 0) {
        return NULL;
    }
    char *err;
    message += strlen("IM:");

    int size = 0;
    operation->port
            = strtoul(read_line_from_array(message, &size, ':'), &err, 10);
    if (*err != '\0' || operation->port < 0) {
        return NULL;
    }
    message += size - 1;
    operation->depotName = read_line_from_array(message, &size, '\n');
    if (size < 1 || message[(size) - 2] != '\n') {
        return NULL;
    }
    if (!check_names(operation->depotName, size)) {
        return NULL;
    }


    operation->threadId = pthread_self();
    operation->message = IM;
    return operation;
}

/**
 * ReceivedOperation*parse_connect
 *
 * @param  {char*} message              : message to parse
 * @return {ReceivedOperation*}
 *      : Allocated and filled ReceivedOperation struct
 */
ReceivedOperation *parse_connect(char *message)
{
    ReceivedOperation *operation = calloc(1, sizeof(ReceivedOperation));

    if (strncmp(message, "Connect:", 8) != 0) {
        return 0;
    }
    char *err;
    message += strlen("Connect:");

    int size = 0;
    operation->port
            = strtoul(read_line_from_array(message, &size, '\n'), &err, 10);
    if (*err != '\0' || operation->port < 0) {
        return NULL;
    }
    if (size < 1 || message[(size) - 2] != '\n') {
        return NULL;
    }

    operation->message = CONNECT;
    return operation;
}

/**
 *
 * @param  {FILE*} input        : file to read from
 * @param  {struct*} Channel    : channel to write to
 * @param  {bool*} firstMessage : first message flag
 * @return {int}                : 0 for ok, 1 otherwise
 */
int parse_input(FILE *input, struct Channel *channel, bool *firstMessage) {
    char inputBuffer[BUFFSIZE];
    memset(inputBuffer, '\0', BUFFSIZE);
    if (!fgets(inputBuffer, BUFFSIZE, input)) {
        return 1;
    }

    ReceivedOperation *operation = calloc(1, sizeof(ReceivedOperation));

    switch (inputBuffer[2]) {
        case 'n':
            operation = parse_connect(inputBuffer);
            break;

        case ':':
            operation = parse_im(inputBuffer);
            break;

        case 'l':
            operation = parse_delivery(inputBuffer);
            break;

        case 't':
            operation = parse_withdrawral(inputBuffer);
            break;

        case 'a':
            operation = parse_transfer(inputBuffer);
            break;

        case 'f':
            operation = parse_deferred(inputBuffer);
            break;

        case 'e':
            operation = parse_execute(inputBuffer);
            break;

        default:
            break;
    }
    if (operation != NULL) {
        if (*firstMessage == true && operation->message != IM) {
            return 1;
        } else {
            *firstMessage = false;
        }
        write_channel(channel, operation);
    }

    return 0;
}

/**
 * void*listener_thread
 *
 * @param  {void*} arg : initial arguments to pass to thread
 */
void *listener_thread(void *arg)
{
    InitArgs *initialArgs = (InitArgs *)arg;
    FILE *readStream = fdopen(initialArgs->readFd, "r");
    FILE *writeStream = fdopen(initialArgs->writeFd, "w");

    ReceivedOperation *operation = calloc(1, sizeof(ReceivedOperation));

    operation->readFrom = readStream;
    operation->writeTo = writeStream;
    operation->threadId = pthread_self();
    operation->message = NEIGHBOUR;

    bool firstMessage = true;

    write_channel(initialArgs->channel, operation);
    while (parse_input(readStream, initialArgs->channel, &firstMessage) != 1) {
        ;
    }
    return NULL;
}
