#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <stdint.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h> 

#include <fcntl.h>

#include <signal.h>

#include <errno.h>
extern int errno;

#include "list_interface.h"

int user_on(client_list* clients,uint16_t client_port,uint32_t client_ip)
{
	int sock;
	char buf[64];
	int bytes;
	struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;

    uint32_t ip;
    uint16_t port;

    client_list* temp;
    temp = clients;
    printf("IN USER ON\n");
	while(temp != NULL)
	{	
		if( (temp->ip != client_ip) || (temp->port != client_port) )
		{	
			if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
			{
				fprintf(stderr,"Error in socket in user_on.\n");
				close(sock);
				return -1;
			}
				
			bzero(&server, sizeof(server));
    		server.sin_family = AF_INET;       /* Internet domain */
    		server.sin_port = htons(temp->port); /* Server port */
  			server.sin_addr.s_addr = htonl(temp->ip);//htonl(temp->ip);

			if (connect(sock, serverptr, sizeof(server)) < 0)
			{
				fprintf(stderr,"Error in connect in user_on.\n");
				close(sock);
				return -1;				
			}

			strcpy(buf,"USER_ON");
			
			bytes = write(sock,buf,strlen(buf)+1);
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write in user_on.\n");
				close(sock);
				return -1;	
			}

			ip = htonl(client_ip);
			port = htons(client_port);

			bytes = write(sock,&ip,sizeof(ip));
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write in user_on.\n");
				close(sock);
				return -1;	
			}

			bytes = write(sock,&port,sizeof(port));
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write in user_on.\n");
				close(sock);
				return -1;	
			}

			close(sock);			
		}

		temp = temp->next_node;
	}
	printf("END OF USER ON\n");
	return 0;
}

int log_on(client_list** clients,int socket)
{	
	int insert = 0;
	uint32_t client_ip=0;
	int bytes = read(socket,&client_ip,sizeof(client_ip));
	if (bytes < 0)
	{	
		fprintf(stderr,"Error in read in log_on.\n");
		return -1;
	}
	//printf("without: %d\n",client_ip );
		
	client_ip = ntohl(client_ip); 
	//printf("with: %d\n",client_ip );		
	struct in_addr ip_addr;
    ip_addr.s_addr = client_ip;
		
	uint16_t client_port;
	bytes = read(socket,&client_port,sizeof(client_port));
	if (bytes < 0)
	{	
		fprintf(stderr,"Error in read in log_on.\n");
		return -1;
	}

	client_port = ntohs(client_port);
	printf("%s %d\n",inet_ntoa(ip_addr),client_port );
	//printf("%d %d\n",client_ip,client_port);
	insert = insert_node(clients,client_port, client_ip);
	if (insert == 1)
		if (user_on(*clients,client_port,client_ip) < 0)
			return -1;	

	return 0;

}

int get_clients(client_list** clients,int socket)
{
	char buf[32];
	int bytes;

	int nodes = counter_nodes(*clients);
	sprintf(buf,"CLIENT_LIST %d",nodes);


	bytes = write(socket,buf,strlen(buf)+1);
	if (bytes < 0)
	{
		fprintf(stderr,"Error in write in get_clients.\n");
		return -1;
	}

	client_list* temp; 
	temp = *clients;

	uint16_t port;
	uint32_t ip;
	while(temp != NULL)
	{	
		ip = htonl(temp->ip);
		bytes = write(socket, &ip , sizeof(ip) );
		if (bytes < 0)
		{
			fprintf(stderr,"Error in write in get_clients.\n");
			return -1 ;
		}

		port = htons(temp->port);
		bytes = write(socket,&port, sizeof(port)) ;
		if (bytes < 0)
		{
			fprintf(stderr,"Error in write in get_clients.\n");
			return -1;
		}
		temp = temp->next_node;		
	}
	return 0;
}

int user_off(client_list* clients,uint16_t client_port,uint32_t client_ip)
{
	int sock;
	char buf[64];
	int bytes;
	struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;

    uint32_t ip;
    uint16_t port;

    client_list* temp;
    temp = clients;
    printf("IN USER OFF\n");
	while(temp != NULL)
	{	
		if( (temp->ip != client_ip) || (temp->port != client_port) )
		{	
			if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
			{
				fprintf(stderr,"Error in socket in user_off.\n");
				close(sock);
				return -1;
			}
				
			bzero(&server, sizeof(server));
    		server.sin_family = AF_INET;       /* Internet domain */
    		server.sin_port = htons(temp->port); /* Server port */
  			server.sin_addr.s_addr = htonl(temp->ip);

			if (connect(sock, serverptr, sizeof(server)) < 0)
			{
				fprintf(stderr,"Error in connect in user_off.\n");
				close(sock);
				return -1;				
			}

			strcpy(buf,"USER_OFF");
			
			bytes = write(sock,buf,strlen(buf)+1);
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write in user_off.\n");
				close(sock);
				return -1;	
			}

			ip = htonl(client_ip);
			port = htons(client_port);

			bytes = write(sock,&ip,sizeof(ip));
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write in user_off.\n");
				close(sock);
				return -1;	
			}

			bytes = write(sock,&port,sizeof(port));
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write in user_off.\n");
				close(sock);
				return -1;	
			}

			close(sock);			
		}

		temp = temp->next_node;
	}
	print_list(clients);
	printf("END OF USER_OFF\n");
	return 0;
}

int log_off(client_list** clients,int socket)
{
	int del = 0;
	uint32_t client_ip=0;
	int bytes = read(socket,&client_ip,sizeof(client_ip));
	if (bytes < 0)
	{	
		fprintf(stderr,"Error in read in log_off.\n");
		return -1;
	}

//	printf("%d\n",client_ip );
		
	client_ip = ntohl(client_ip); 
		
	struct in_addr ip_addr;
    ip_addr.s_addr = client_ip;
		
	uint16_t client_port;
	bytes = read(socket,&client_port,sizeof(client_port));
	if (bytes < 0)
	{	
		fprintf(stderr,"Error in read in log_off.\n");
		return -1;
	}
	client_port = ntohs(client_port);

//	printf("%d %d\n",client_ip, client_port );
		
	printf("%s %d\n",inet_ntoa(ip_addr),client_port );

	del = delete_node(clients,client_port, client_ip);
	print_list(*clients);
	if (del == 1)
	{	if (user_off(*clients,client_port,client_ip) < 0)
			return -1;
	}
	else
	{
		printf("ERROR_IP_PORT_NOT_FOUND_IN_LIST\n");
		return -2;
	}	

	return 0;
}
