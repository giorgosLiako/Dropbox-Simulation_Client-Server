
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "list.h"

/*function to counter the nodes of the list */
int counter_nodes(client_list* L )
{
	client_list* temp;
	temp = L;
	int counter = 0;
	while(temp != NULL)
	{
		counter++;
		temp = temp->next_node;
	}
	return counter;
}

/*function to search the list if exist one specific client and return him otherwise return NULL*/
struct list_node* search_list(client_list *L , uint16_t port ,uint32_t ip)
{
	client_list* M ;
	M = L;
	while( M != NULL)
	{
		if ( (M->port == port) && (M->ip == ip) )/*if the specific client found return him*/
			return M;
		M = M->next_node;
	} 
	return NULL;
}

/*function to insert a new client in the list */
int insert_node(client_list ** L , uint16_t port , uint32_t ip)
{
	/*search if the client already exist in the list */
	if ( search_list(*L,port,ip) != NULL)
		return 0;

	struct list_node * temp;

	/*make a new node */
	temp = malloc(sizeof(struct list_node));
	if (temp == NULL)
	{
		fprintf(stderr,"Error in malloc\n");
		return -1;
	}
	/*set the node with the informations */
	temp->ip = ip;
	temp->port = port;
	temp->next_node = NULL;

	/*if the list is empty make the new node the first node */
	if (*L == NULL)
	{
		*L = temp;
	}
	else /*if the list is not empty*/
	{	/*make the new node points to the start of the list and the pointer to start of the list point to the new node */
		temp->next_node = *L;
		*L = temp;
	}
	return 1;
}


/*function to delete a specific client from the list*/
int delete_node(client_list** L , uint16_t port , uint32_t ip )
{
	struct list_node* temp;
	while (*L != NULL)
	{	/*search the list to find the specific client*/
		if ( ((*L)->port == port) && ((*L)->ip == ip) )
		{	
			temp = *L; /*put the node in a temporary pointer*/
			*L = (*L)->next_node; /*make the previous node points to the next node of the deleted node*/
			free(temp); /*delete the node*/
			return 1;
		}
		else
			L = &((*L)->next_node); /*go to the next node */
	}
	return 0;
}

/*function to free the allocated memory for the list*/
void destroy_list(client_list** L)
{
	struct list_node *temp;
	while(*L != NULL) /*delete each node and go to the next one */
	{	
		temp = *L;
		*L = (*L)->next_node;
		free(temp);
	}
}

/*function to print the client_list */
void print_list(client_list * L )
{
	client_list* temp;

	temp = L;
	if (temp != NULL)
		printf("LIST: \n");

	while(temp != NULL)
	{
		uint32_t mir_ip = htonl(temp->ip);/*take the network byte form of the ip to get the hostname of the client*/
		struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET);
		
		printf("IP: %s PORT: %d\n",h->h_name,temp->port);/*print the informations*/
		temp = temp->next_node;
	}
}