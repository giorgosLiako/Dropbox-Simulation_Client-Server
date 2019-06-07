/*file that manages the buffer*/

#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include "buffer.h"

/*global variables which are declared in dropbox_client.c*/
extern pthread_mutex_t buffer_mtx;
extern pthread_cond_t cond_nonempty;
extern pthread_cond_t cond_nonfull;

/*function to initialize the buffer*/
void initialize_buffer(buffer* b , int buffer_size)
{
	/*make an array of pointers to buffer_items*/
	b->items = (buffer_item**) malloc(buffer_size * sizeof(buffer_item*));
	if (b->items == NULL)
	{
		fprintf(stderr, "Error in malloc in initialize_buffer.\n");
		return;
	}
	/*initialize the pointers */
	for (int i = 0 ; i < buffer_size ; i++)
		b->items[i] = NULL;
	
	/*initialize the variables */
	b->start = 0;
	b->end = -1;
	b->count = 0;
	b->buffer_size = buffer_size;

}

/*function to free the allocated memory of the buffer*/
void destroy_buffer(buffer * b)
{
	for (int i = 0 ; i < b->buffer_size ; i++)
		if ( b->items[i] != NULL)
			free(b->items[i]);/*if a pointer is not NULL free it*/
	
	free(b->items); /*free the array of pointers*/
}

/*function to place a buffer_item in buffer*/
void place(buffer * b, buffer_item * data) 
{
	pthread_mutex_lock(&buffer_mtx); /*lock the buffer*/
	/*if the buffer is full block in the condition variable con_nonfull until there is available space*/
	while (b->count >= b->buffer_size) 
	{
		pthread_cond_wait(&cond_nonfull, &buffer_mtx);
	}

	b->end = (b->end + 1) % b->buffer_size;/*find the index of the cell that the buffer_item will be placed*/
	b->items[b->end] = data; /*place the buffer_item*/
	b->count++; /*increase the count of the items in buffer */

	pthread_mutex_unlock(&buffer_mtx); /*unlock the buffer*/
}

/*function to obtain a buffer_item from buffer */
buffer_item* obtain(buffer * b)
{
	buffer_item* data = NULL;
	pthread_mutex_lock(&buffer_mtx); /*lock the buffer */
	/*if the buffer is empty block in the condition variable con_nonempty until there is available item to obtain*/
	while (b->count <= 0) 
	{
		pthread_cond_wait(&cond_nonempty, &buffer_mtx);
	}

	data = b->items[b->start]; /*take the item that points the index start */
	b->items[b->start] = NULL;/*initialize the pointer of this index*/
	
	b->start = (b->start + 1) % b->buffer_size; /*make start points to the next item*/
	b->count--;

	pthread_mutex_unlock(&buffer_mtx); /*unlock the buffer */
	
	return data; /*return the obtained buffer_item */
}