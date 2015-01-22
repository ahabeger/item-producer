#include <stdio.h>	     // printf
#include "spmc_producer.h" // ...
#include <stdlib.h>      // malloc, exit
	
	
	// used as the item to be put into the queue
	int item;

/**
 * the function called by the producer
 */ 
bool produce (void* structure, int** produced_item) {
	if (item < 5000000) {
		*produced_item = (int*) malloc(sizeof(int));
		**produced_item = item;
		item++;
		return true;
	} else {
		return false;
	}
}

/**
 * wat is called by the consumer threads
 */ 
void get_items (producer_t* producer) {
	
	int items_consumed = 0;
	int* consumed_item = NULL;
	
	spmc_producer_consumer_check_in (producer);
	
	while (spmc_producer_get_item (producer, (void**) &consumed_item)) {
		items_consumed++;
		fprintf (stdout, "Processed item # %d with a value of %d\n", items_consumed, *consumed_item);
		free (consumed_item);
	}
	consumed_item = NULL;
	
	spmc_producer_consumer_check_out (producer);
}

int main () {
	
	item = 0;
	
	producer_t* producer = spmc_producer_new (1024, (bool (*)(void *, void*)) produce, NULL);
	
	spmc_producer_start (producer);
	get_items (producer);
	spmc_producer_wait (producer);
	spmc_producer_free (producer);
	return 0;
}
