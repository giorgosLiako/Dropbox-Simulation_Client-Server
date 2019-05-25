
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "list.h"

int counter_nodes(client_list* L ,uint16_t port ,uint32_t ip )
{
	client_list* temp;
	temp = L;
	int counter = 0;
	while(temp != NULL)
	{
		if ( (temp->ip != ip ) || (temp->port != port) )
			counter++;
		temp = temp->next_node;
	}
	return counter;
}


struct list_node* search_list(client_list *L , uint16_t port ,uint32_t ip)
{
	client_list* M ;
	M = L;
	while( M != NULL)
	{
		if ( (M->port == port) && (M->ip == ip) )
			return M;
		M = M->next_node;
	} 
	return NULL;
}

int insert_node(client_list ** L , uint16_t port , uint32_t ip)
{
	if ( search_list(*L,port,ip) != NULL)
		return 0;

	struct list_node * temp;

	temp = malloc(sizeof(struct list_node));
	if (temp == NULL)
	{
		fprintf(stderr,"Error in malloc\n");
		return -1;
	}

	temp->ip = ip;
	temp->port = port;
	temp->next_node = NULL;

	if (*L == NULL)
	{
		*L = temp;
	}
	else
	{
		temp->next_node = *L;
		*L = temp;
	}
	return 1;
}


void delete_node(client_list** L , uint16_t port , uint32_t ip )
{
	struct list_node* temp;
	while (*L != NULL)
	{
		if ( ((*L)->port == port) && ((*L)->ip == ip) )
		{
			temp = *L;
			*L = (*L)->next_node;
			free(temp);
			return;
		}
		else
			L = &((*L)->next_node);
	}
}

void destroy_list(client_list** L)
{

	struct list_node *temp;
	while(*L != NULL)
	{	
		temp = *L;
		*L = (*L)->next_node;
		free(temp);
	}
}