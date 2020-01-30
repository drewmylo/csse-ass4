#include "error.h"
#include <stdio.h>

/**
 * Returns the corresponding error message.
 *
 * Parameters:
 *      r   :   the code to get the error message for
 */
const char *error_message(Error error) {
    switch (error) {
        case NORMAL:
            {
                return "";
            }                   // All is well
        case BAD_ARGS:
            {
                return "Usage: 2310depot name {goods qty}";
            }
        case BAD_NAME:
            {
                return "Invalid name(s)";
            }
        case BAD_QTY:
            {
                return "Invalid quantity";
            }
        default:
            {
                return "Something went very wrong";
            }
    }
}

/**
 * Prints the correct error string to stderr and returns error code.
 * Parameters:
 *        error Error code to print and return.
 */
Error exit_with_error(Error error) {
    if (error != NORMAL) {
        fprintf(stderr, "%s\n", error_message(error));
    }
    return error;
}
