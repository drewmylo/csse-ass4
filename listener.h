#include "utils.h"
#include "channel.h"

bool check_names(char *name, int size);
int parse_quant_type(char *message, ReceivedOperation *product, bool transfer);
ReceivedOperation *parse_execute(char *message);
ReceivedOperation *parse_delivery(char *message);
ReceivedOperation *parse_withdrawral(char *message);
ReceivedOperation *parse_transfer(char *message);
ReceivedOperation *parse_deferred(char *message);
ReceivedOperation *parse_IM(char *message);
ReceivedOperation *parse_connect(char *message);
int parse_input(FILE *input, struct Channel *channel);
void *listener_thread(void *arg);