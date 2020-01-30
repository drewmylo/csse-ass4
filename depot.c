#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <ctype.h>
#include "depot.h"
#include "error.h"
#include "utils.h"
#include "channel.h"
#include "listener.h"

#define BUFFSIZE 256

pid_t gettid(void);

static sigset_t signalMask; /* signals to block         */

/**
 *
 * @param  {Depot*} depot :  main depot struct
 */
void print_names(Depot *depot)
{
    printf("Goods:\n");
    for (int i = 0; i < depot->inventoryCount; i++) {
        if (depot->inventory[i].productQuantity != 0) {
            printf("%s %d\n", depot->inventory[i].productName,
                    depot->inventory[i].productQuantity);
        }
    }
    fflush(stdout);
}

/**
 *
 * @param  {Depot*} depot :  main depot struct
 * @param  {pid_t} threadId : the id of the thread that is listening
 * to the neighbour
 * @return {int}             : the 0-based index of the neighbour struct
 */
int neighbour_from_pid(Depot *depot, pid_t threadId)
{
    for (int i = 0; i < depot->neighbourCount; i++) {
        if (depot->neighbours[i].threadId == threadId) {
            return i;
        }
    }
    return -1;
}

/**
 *
 * @param  {Depot*} depot :  main depot struct main depot struct
 */
void print_neighbours(Depot *depot)
{
    printf("Neighbours:\n");
    for (int i = 0; i < depot->neighbourCount; i++) {
        printf("%s\n", depot->neighbours[i].name);
    }
    fflush(stdout);
}

/**
 *
 * @param  {void*} a : the first product to compare
 * @param  {void*} b : the second product to compare
 * @return {int}     : the result of the comparison
 */
int compare_product(const void *a, const void *b)
{
    Product *productA = (Product *)a;
    Product *productB = (Product *)b;

    return strcmp(productA->productName, productB->productName);
}

/**
 *
 * @param  {void*} a : the first neighbour to compare
 * @param  {void*} b : the second neigbour to compare
 * @return {int}     : the result of the comparison
 */
int compare_neighbour(const void *a, const void *b)
{
    Neighbour *neighbourA = (Neighbour *)a;
    Neighbour *neighbourB = (Neighbour *)b;

    return strcmp(neighbourA->name, neighbourB->name);
}

/**
 *
 * @param  {Depot*} depot :  main depot struct
 */
void print_names_and_neighbours(Depot *depot)
{
    qsort(depot->inventory, depot->inventoryCount, sizeof(Product),
            compare_product);
    print_names(depot);
    qsort(depot->neighbours, depot->neighbourCount, sizeof(Neighbour),
            compare_neighbour);
    print_neighbours(depot);
}

/**
 *
 * @param  {char*} name   :  the name of the product
 * @param  {Depot*} depot :  main depot struct
 * @return {int}          :  the 0-based position of the product
 */
int find_product(char *name, Depot *depot)
{
    for (int i = 0; i < depot->inventoryCount; i++) {
        if (strcmp(depot->inventory[i].productName, name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 *
 * @param  {ReceivedOperation*} product : the product to add
 * @param  {Depot*} depot :  main depot struct
 *
 */
void add_product(ReceivedOperation *product, Depot *depot)
{
    int productPos = -1;

    productPos = find_product(product->productName, depot);
    if (productPos != -1) {
        depot->inventory[productPos].productQuantity +=
                product->productQuantity;
    } else {
        depot->inventoryCount++;
        depot->inventory = realloc(depot->inventory,
                (depot->inventoryCount * sizeof(Product)));
        depot->inventory[depot->inventoryCount - 1].productName
                = product->productName;
        depot->inventory[depot->inventoryCount - 1].productQuantity
                = product->productQuantity;
    }
}

/**
 *
 * @param  {ReceivedOperation*} product : the product to remove
 * @param  {Depot*} depot :  main depot struct
 */
void remove_product(ReceivedOperation *product, Depot *depot)
{
    int pos = find_product(product->productName, depot);
    if (pos != -1) {
        depot->inventory[pos].productQuantity -=
                product->productQuantity;
    } else {
        product->productQuantity *= -1;
        add_product(product, depot);
    }
}

/**
 *
 * @param  {ReceivedOperation*} operation : the operation to add to the
 *     deferred list
 * @param  {Depot*} depot :  main depot struct
 */
void add_deferred(ReceivedOperation *operation, Depot *depot)
{
    depot->defferedCount++;
    depot->defferedOps = realloc(depot->defferedOps, (depot->defferedCount
            * sizeof(ReceivedOperation)));
    memcpy(&depot->defferedOps[depot->defferedCount - 1], operation,
            sizeof(ReceivedOperation));
}

/**
 *
 * @param  {ReceivedOperation*} deferredOps : the list of deferred operations
 * @param  {int} pos                        : the position of the operation to
 *     remove
 * @param  {int} count                      : the number of operations in the
 *     array
 */
void remove_single_op(ReceivedOperation *deferredOps, int pos, int count)
{
    for (int x = pos; x < (count - 1); x++) {
        memcpy(&deferredOps[x], &deferredOps[x + 1],
                sizeof(ReceivedOperation));
    }
}

/**
 *
 * @param  {ReceivedOperation*} product : the product to send
 * @param  {Depot*} depot :  main depot struct
 */
bool send_product(ReceivedOperation *product, Depot *depot)
{
    for (int i = 0; i < depot->neighbourCount; i++) {
        if (strcmp(depot->neighbours[i].name, product->depotName) == 0) {
            fprintf(depot->neighbours[i].writeTo,
                    "Deliver:%d:%s\n",
                    product->productQuantity, product->productName);
            fflush(depot->neighbours[i].writeTo);
            return true;
        }
    }
    return false;
}

/**
 *
 * @param  {unsigned} int : the key of the operation/s to execute
 * @param  {Depot*} depot :  main depot struct
 */
void execute(unsigned int key, Depot *depot)
{
    for (int i = 0; i < depot->defferedCount; i++) {
        if (key == depot->defferedOps[i].key
                && depot->defferedOps[i].deferred == true) {
            depot->defferedOps[i].deferred = false;
            write_channel(depot->channel, &depot->defferedOps[i]);
        }
    }
}

/**
 *
 * @param  {unsigned int} port : the port of the neighbour
 * @param  {Depot*} depot : main depot struct
 * @return {bool}         : true if already connected, false otherwise.
 */
bool neighbour_connected(unsigned int port, Depot *depot)
{
    for (int i = 0; i < depot->neighbourCount; i++) {
        if (depot->neighbours[i].port == port) {
            return true;
        }
    }
    return false;
}

/**
 *
 * @param  {Depot*} depot :  main depot struct
 * @param  {pid_t} threadId :
 * the id of the thread that is connected to that particular neighbour
 * @param  {char*} name      : the name of the neighbour
 * @param  {unsigned int} port    : neighbour's port number
 */
void add_neighbour_name_port(Depot *depot, pid_t threadId, char *name,
        unsigned int port)
{
    int pos = neighbour_from_pid(depot, threadId);
    if (pos >= 0) {
        depot->neighbours[pos].port = port;
        depot->neighbours[pos].name = name;
    }
}

/**
 *
 * @param  {InitArgs*} args   : the initial arguments to pass to the new thread
 * @param  {void**} func : the function to run
 */
void create_thread(InitArgs *args, void (*func(void *arg)))
{
    pthread_t threadId;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&threadId, &attr, func, (void *)args);
    pthread_attr_destroy(&attr);
}

/**
 *
 * @param  {Depot*} depot :  main depot struct
 * @param  {pid_t} threadId : the id of the thread that is
 * listening to the neighbour
 * @param  {FILE*} writeTo   : the file descriptor to write to
 * @param  {FILE*} readFrom  : the file descriptor to read from
 */
void add_neighbour(Depot *depot, pid_t threadId, FILE *writeTo, FILE *readFrom)
{
    depot->neighbourCount++;
    int pos = (depot->neighbourCount - 1);
    depot->neighbours = realloc(depot->neighbours,
            (depot->neighbourCount * sizeof(Neighbour)));
    depot->neighbours[pos].threadId = threadId;
    depot->neighbours[pos].writeTo = writeTo;
    depot->neighbours[pos].readFrom = readFrom;
}

/**
 *
 * @param  {Depot*} depot :  main depot struct
 * @param  {pid_t} threadId : the id of the thread listenting to
 * the neighbour to remove
 */
void remove_neighbour(Depot *depot, pid_t threadId)
{
    int pos = neighbour_from_pid(depot, threadId);
    for (int x = pos; x < (depot->neighbourCount - 1); x++) {
        memcpy(&depot->neighbours[x], &depot->neighbours[x + 1],
                sizeof(Neighbour));
    }

    depot->neighbourCount--;
    depot->neighbours = realloc(depot->neighbours,
            (depot->neighbourCount * sizeof(Neighbour)));
}

/**
 *
 * @param  {unsigned int} port      : this depot's port
 * @param  {char*} name             : this depot's name
 * @param  {FILE*} neighbourStreams : the file to write the IM message to
 */
void send_im(unsigned int port, char *name, FILE *neighbourStreams)
{
    fprintf(neighbourStreams, "IM:%u:%s\n", port, name);
    fflush(neighbourStreams);
}

/**
 *
 * @param  {Depot*} depot :  main depot struct
 * @param  {unsigned int} port : the port to connect to
 */
void connect_to_neighbour(Depot *depot, unsigned int port)
{

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in outboundAddress;
    outboundAddress.sin_port = htons(port);
    outboundAddress.sin_addr.s_addr = 0;
    outboundAddress.sin_addr.s_addr = INADDR_ANY;
    outboundAddress.sin_family = AF_INET;
    if (connect(fd, (struct sockaddr *)&outboundAddress,
            sizeof(struct sockaddr_in)) == -1) {
        printf("\n NO CONNEC\n");
    }
    FILE *stream = fdopen(fd, "w");
    fflush(stream);
    InitArgs *args = calloc(1, sizeof(InitArgs));
    args->writeFd = fd;
    args->readFd = dup(fd);
    args->channel = depot->channel;
    create_thread(args, listener_thread);
}

/**
 *
 * @param depot     main depot struct
 * @param operation operation to work with
 */
void check_send_im(Depot *depot, ReceivedOperation *operation) {
    if (!neighbour_connected(operation->port, depot)) {
        add_neighbour_name_port(depot,
                operation->threadId,
                operation->depotName, operation->port);
    } else {
        remove_neighbour(depot, operation->threadId);
    }
}

/**
 * void*responder_thread
 *
 * @param  {void*} arg : the argument struct
 */
void *responder_thread(void *arg) {
    InitArgs *initialArgs = (InitArgs *)arg;
    ReceivedOperation *operation = {0};
    Depot *depot = initialArgs->depot;
    while (1) {
        if ((read_channel(initialArgs->depot->channel, (void **)&operation))) {
            if (operation->deferred) {
                add_deferred(operation, depot);
            } else {
                switch (operation->message) {
                    case CONNECT:
                        if (!neighbour_connected(operation->port, depot)) {
                            connect_to_neighbour(depot, operation->port);
                        }
                        break;
                    case IM:
                        check_send_im(depot, operation);
                        break;
                    case DELIVER:
                        add_product(operation, depot);
                        break;
                    case WITHDRAW:
                        remove_product(operation, depot);
                        break;
                    case TRANSFER:
                        if (send_product(operation, depot)) {
                            remove_product(operation, depot);
                        }
                        break;
                    case EXECUTE:
                        execute(operation->key, depot);
                        break;
                    case NEIGHBOUR:
                        add_neighbour(depot, operation->threadId,
                                operation->writeTo, operation->readFrom);
                        send_im(depot->port, depot->name,
                                operation->writeTo);
                        break;
                    case HUPPED:
                        print_names_and_neighbours(depot);
                    default:
                        break;
                }
            }
        }
    }
}

/**
 * void*signal_thread
 *
 * @param  {void*} arg : the argument struct
 */
void *signal_thread(void *arg)
{
    InitArgs *initialArgs = (InitArgs *)arg;
    int sigCaught; /* signal caught       */
    int rc;     /* returned code       */
    ReceivedOperation *operation = calloc(1, sizeof(ReceivedOperation));
    operation->message = HUPPED;

    while (1) {
        rc = sigwait(&signalMask, &sigCaught);
        if (rc != 0) {
            /* handle error */
        }
        switch (sigCaught) {
            case SIGHUP: /* process SIGINT  */
                write_channel(initialArgs->channel, operation);
                break;
        }
    }
    return NULL;
}

/**
 *
 * @param  {Depot*} depot :  main depot struct
 * @return {int}          :  returns 0 if all went well
 */
int open_socket(Depot *depot)
{
    struct addrinfo *ai = 0;
    struct addrinfo addressStruct;
    memset(&addressStruct, 0, sizeof(struct addrinfo));
    addressStruct.ai_family = AF_INET; 
    addressStruct.ai_socktype = SOCK_STREAM;
    addressStruct.ai_flags = AI_PASSIVE; 

    getaddrinfo("localhost", 0, &addressStruct, &ai);    
    // create a socket and bind it to a port
    int serv = socket(AF_INET, SOCK_STREAM, 0); 
    bind(serv, (struct sockaddr *)ai->ai_addr, sizeof(struct sockaddr));
     
    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(serv, (struct sockaddr *)&ad, &len));
       
    printf("%u\n", ntohs(ad.sin_port));
    fflush(stdout);
    depot->port = ntohs(ad.sin_port);

    listen(serv, 10);
    int connFd;
    while (1) {
        if ((connFd = accept(serv, 0, 0), connFd) >= 0) {
            InitArgs *args = calloc(1, sizeof(InitArgs));
            args->writeFd = connFd;
            args->readFd = dup(connFd);
            args->channel = depot->channel;
            create_thread(args, listener_thread);
        }
    }
    return 0;
}

/**
 * Depot* check_and_load_arguments
 *
 * @param  {int} argc       : number of arguments received
 * @param  {char* []} const : array of argument strings
 */
Depot *check_and_load_arguments(int argc, char const *argv[])
{
    //argc must be even
    if (argc % 2 == 1) {
        exit(exit_with_error(BAD_ARGS)); //todo
    }

    Depot *depot = calloc(1, sizeof(Depot));
    depot->inventory = calloc((argc - 2), sizeof(Product));
    depot->inventoryCount = ((argc - 2) / 2);
    int size = 0;
    depot->name = read_line_from_array((char *)argv[1], &size, '\0');
    if (!check_names(depot->name, size)) {
        exit(exit_with_error(BAD_NAME)); //todo
    }

    int positionInInventory = 0;
    char *err1;
    for (int i = 2; i < argc; i = i + 2) { //starting at 2 so as to...
        depot->inventory[positionInInventory].productName
                = read_line_from_array((char *)argv[i], &size, '\0');
        if (!check_names(depot->inventory[positionInInventory].productName,
                size)) {
            exit(exit_with_error(BAD_NAME)); //todo
        }
        depot->inventory[positionInInventory].productQuantity =
                strtoul(argv[i + 1], &err1, 10);

        if (*err1 != '\0'
                || depot->inventory[positionInInventory].productQuantity
                < 1) {
            exit(exit_with_error(BAD_QTY)); //todo
        }
        positionInInventory++;
    }
    return depot;
}

/**
 *
 * @param  {int} argc       : number of arguments received
 * @param  {char* []} const : array of argument strings
 * @return {int}            : returns exit status
 */
int main(int argc, char const *argv[])
{

    sigemptyset(&signalMask);
    sigaddset(&signalMask, SIGHUP);
    sigaddset(&signalMask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &signalMask, NULL);

    pthread_mutex_t *m;
    sem_t *semaphore;
    m = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    semaphore = (sem_t *)malloc(sizeof(sem_t));

    Depot *depot = check_and_load_arguments(argc, argv);
    /* code */

    pthread_mutex_init(m, NULL); // initialize mutex m
    sem_init(semaphore, 0, 1);

    InitArgs *args = calloc(1, sizeof(InitArgs));

    args->depot = depot;
    struct Channel channel = new_channel();
    channel.mutex = m;
    channel.semaphore = semaphore;
    depot->channel = &channel;
    args->channel = &channel;

    create_thread(args, signal_thread);
    create_thread(args, responder_thread);
    open_socket(depot);

    return 0;
}
