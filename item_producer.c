#include "spmc_producer.h" // ...
#include <pthread.h>
#include "liblfds611.h" // liblfds
#include <stdio.h>	   // printf

struct producer_struct {
	bool   more_items;
	size_t consumer_count;
	size_t consumers_dequeing;
	void*  structure;
	struct lfds611_queue_state* queue;
	producer_func_t* producer_func;
	pthread_t producer_thread;
	pthread_mutex_t lock_until_done;
	pthread_mutex_t queue_lock; 
	pthread_cond_t producer_cond;
	pthread_cond_t consumer_cond;
};

/**
 * produce an individual item, blocks until item has been enqueued
 */ 
void spmc_produce_item (producer_t* producer, void* item) {
		
	bool item_enqueued = (bool) lfds611_queue_enqueue (producer->queue, item);
	
	// if above enqueue failed keep retyring until success
	// one wait and retry per iteration
	while ( ! item_enqueued) {
		pthread_mutex_lock (&producer->queue_lock);
		pthread_cond_signal (&producer->consumer_cond);
		pthread_cond_wait  (&producer->producer_cond, &producer->queue_lock);
		pthread_mutex_unlock (&producer->queue_lock);
		item_enqueued = (bool) lfds611_queue_enqueue (producer->queue, item);
	}
	pthread_cond_broadcast (&producer->consumer_cond);
}

/**
 * produces items until supplied function returns false
 */ 
void spmc_produce_items (producer_t* producer) {
	
	void* item = NULL;
	
	lfds611_queue_use (producer->queue);
	
	// produces items until the producer func returns false
	// produces one item and enqueues it per iteration
	while (producer->producer_func (producer->structure, &item)) {
		spmc_produce_item (producer, item);
	}
	
	// done producing, cleaning up
	__atomic_store_n (&producer->more_items, false, __ATOMIC_RELAXED);
		
	item = NULL;
	
	// ensures that no consumers are stuck on the wait,loops until all consumers have exited
	// sends one broadcast per iteration
	while (0 < __atomic_load_n (&producer->consumers_dequeing, __ATOMIC_RELAXED)) {
		pthread_cond_broadcast (&producer->consumer_cond);
		pthread_mutex_lock (&producer->queue_lock);
		pthread_mutex_unlock (&producer->queue_lock);
	}	
	pthread_exit (NULL);
}

producer_t* spmc_producer_new (size_t items_buffered, producer_func_t* producer_func, void* structure) {
	
	producer_t* result = malloc (sizeof (producer_t));
	
	result->consumer_count = 0;
	result->structure = structure;
	result->consumers_dequeing = 0;
	
	lfds611_queue_new (&(result->queue), items_buffered);
	
	result->producer_func = producer_func;
	
	pthread_mutex_init (&result->lock_until_done, NULL);
	pthread_mutex_lock (&result->lock_until_done);
	pthread_mutex_init (&result->queue_lock, NULL);
	pthread_cond_init (&result->producer_cond, NULL);
	pthread_cond_init (&result->consumer_cond, NULL);
	result->more_items = true;
	
	return result;
}

void spmc_producer_start (producer_t* producer) {
	pthread_create (&producer->producer_thread, NULL, (void *) &spmc_produce_items, (void *) producer);
}

void spmc_producer_consumer_check_in (producer_t* producer) {
	lfds611_queue_use (producer->queue);
	__atomic_add_fetch (&producer->consumer_count, 1, __ATOMIC_RELAXED);
}

bool spmc_producer_get_item (producer_t* producer, void** item) {
	
	__atomic_add_fetch (&producer->consumers_dequeing, 1, __ATOMIC_RELAXED);
	
	bool have_item = (bool) lfds611_queue_dequeue (producer->queue, item);
	
	// if above dequeue failed keep retyring until success
	// one wait and retry per iteration
	while (__atomic_load_n (&producer->more_items, 1) && ! have_item) {
		pthread_mutex_lock (&producer->queue_lock);
		pthread_cond_signal (&producer->producer_cond);
		pthread_cond_wait  (&producer->consumer_cond, &producer->queue_lock);
		pthread_mutex_unlock (&producer->queue_lock);
		have_item = (bool) lfds611_queue_dequeue (producer->queue, item);
	}
	pthread_cond_signal (&producer->producer_cond);
	
	__atomic_sub_fetch (&producer->consumers_dequeing, 1, __ATOMIC_RELAXED);
	
	return have_item;
}

void spmc_producer_consumer_check_out (producer_t* producer) {
	pthread_mutex_lock (&producer->queue_lock);
	pthread_cond_broadcast (&producer->consumer_cond);
	pthread_mutex_unlock (&producer->queue_lock);
	
	// only once all consumers are out is the producer considered finished
	if (0 == __atomic_sub_fetch (&producer->consumer_count, 1, __ATOMIC_RELAXED)) {
		pthread_mutex_unlock (&producer->lock_until_done);
	}
}

void spmc_producer_wait (producer_t* producer) {
	// this lock will be locked until all consumers have checked out
	pthread_mutex_lock   (&producer->lock_until_done);
	pthread_mutex_unlock (&producer->lock_until_done);
}

bool spmc_producer_finished (producer_t* producer) {
	if (NULL == producer) {
		return false;
	}
	pthread_cond_broadcast (&producer->consumer_cond);
	return 0 != __atomic_load_n (&producer->consumer_count, 1);
}

bool spmc_producer_free (producer_t* producer) {
	if (NULL == producer) {
		return false;
	} else {
		lfds611_queue_delete (producer->queue, NULL, NULL);
		pthread_mutex_destroy (&producer->queue_lock);
		pthread_mutex_destroy (&producer->lock_until_done);
		pthread_cond_destroy (&producer->producer_cond);
		pthread_cond_destroy (&producer->consumer_cond);
		free (producer);
		producer = NULL;
		return true;
	}
}
