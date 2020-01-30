#include "channel.h"
#include "queue.h"
#include <pthread.h>
#include <semaphore.h>

/*
 * Creates (and returns) a new, empty channel, with no data in it.
 */

struct Channel new_channel(void) {
    struct Channel output;
    output.inner = new_queue();
    return output;
}

/*
 * Destroys an old channel. Takes as arguments a pointer to the channel, as
 * well as a function to use to clean up any elements left in the channel (for
 * example free). If the function provided is NULL, no clean up will be
 * performed on these elements.
 */
void destroy_channel(struct Channel *channel, void (*clean) (void *)) {
    destroy_queue(&channel->inner, clean);
}

/*
 * Attempts to write a piece of data to the channel. Takes as arguments a
 * pointer to the channel, and the data being written. Returns true if the
 * attempt was successful, and false if the channel was full and unable to be
 * written to.
 */
bool write_channel(struct Channel *channel, void *data) {
    pthread_mutex_lock(channel->mutex);
    bool output = write_queue(&channel->inner, data);
    pthread_mutex_unlock(channel->mutex);
    sem_post(channel->semaphore);

    return output;
}

/*
 * Attempts to read a piece of data from the channel. Takes as arguments a
 * pointer to the channel, and a pointer to where the data should be stored on
 * a successful channel. On success, returns true and sets *output to the read
 * data. On failure (due to empty channel), returns false and does not touch
 * *output.
 */
bool read_channel(struct Channel * channel, void **out) {
    sem_wait(channel->semaphore);

    pthread_mutex_lock(channel->mutex);
    bool output = read_queue(&channel->inner, out);
    pthread_mutex_unlock(channel->mutex);
    return output;
}
