#ifndef spmc_producer
#define spmc_producer

#include <stdbool.h> // ...
#include <stddef.h>  // size_t

typedef bool (producer_func_t) (void*, void*);

struct producer_struct;
typedef struct producer_struct producer_t;

/**
 * initializes the producer
 * genaric_producer
 * items buffered - the # of items in the internal queue
 * producer_func - the source for the items to be produced
 */
producer_t* spmc_producer_new (size_t items_buffered, producer_func_t* producer_func, void* structure);

/**
 * starts the producer
 * 
 * returns true if success
 */ 
void spmc_producer_start (producer_t* producer);


void spmc_producer_consumer_check_in (producer_t* producer);

/**
 * gets an individual item from the producer
 * 
 * returns true if the item is valid, returns false if source and queue are exhausted
 */ 
bool spmc_producer_get_item (producer_t* producer, void** item);


void spmc_producer_consumer_check_out (producer_t* producer);

/**
 * blocks until the producer and consumers have completed
 */ 
void spmc_producer_wait (producer_t* producer);

/**
 * returns if the producer and consumers have completed
 */ 
bool spmc_producer_finished (producer_t* producer);

/**
 * Free any structures utilized by the producer func before this
 * 
 * not thread safe
 */
bool spmc_producer_free (producer_t* producer);

#endif
