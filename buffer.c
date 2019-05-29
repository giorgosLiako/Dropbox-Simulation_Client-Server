
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include "buffer.h"

extern pthread_mutex_t mtx;
extern pthread_cond_t cond_nonempty;
extern pthread_cond_t cond_nonfull;

void initialize_buffer(buffer* b , int buffer_size)
{
	b->items = (buffer_item**) malloc(buffer_size * sizeof(buffer_item*));
	if (b->items == NULL)
	{
		fprintf(stderr, "Error in malloc in initialize_buffer.\n");
		return;
	}
	for (int i = 0 ; i < buffer_size ; i++)
		b->items[i] = NULL;

	b->start = 0;
	b->end = -1;
	b->count = 0;
	b->buffer_size = buffer_size;

}

void destroy_buffer(buffer * b)
{
	for (int i = 0 ; i < b->buffer_size ; i++)
		if ( b->items[i] != NULL)
			free(b->items[i]);
	
	free(b->items);
}

void place(buffer * b, buffer_item * data) 
{
	pthread_mutex_lock(&mtx);
	while (b->count >= b->buffer_size)
	{
		printf(">> Found Buffer Full \n");
		pthread_cond_wait(&cond_nonfull, &mtx);
	}

	b->end = (b->end + 1) % b->buffer_size;
	b->items[b->end] = data;
	b->count++;
	pthread_mutex_unlock(&mtx);
	printf("Placed item\n");
}


buffer_item* obtain(buffer * b)
{
	buffer_item* data = NULL;
	pthread_mutex_lock(&mtx);

	while (b->count <= 0) 
	{
		printf(">> Found Buffer Empty \n");
		pthread_cond_wait(&cond_nonempty, &mtx);
	}

	data = b->items[b->start];
	b->items[b->start] = NULL;
	
	b->start = (b->start + 1) % b->buffer_size;
	b->count--;

	pthread_mutex_unlock(&mtx);
	printf("Obtained item\n");
	return data;
}