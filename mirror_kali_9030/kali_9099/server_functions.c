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

/*function that informs the others clients that one client is on*/
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
    /*traverse all the client-list*/
	while(temp != NULL)
	{	/*if the client-node is not the client that connected now send the user_on*/
		if( (temp->ip != client_ip) || (temp->port != client_port) )
		{	/*make a new socket*/
			if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
			{
				fprintf(stderr,"Error in socket in user_on.\n");
				close(sock);
				return -1;
			}
			/*set the struct*/
			bzero(&server, sizeof(server));
    		server.sin_family = AF_INET;       /* Internet domain */
    		server.sin_port = htons(temp->port); /* Server port */
  			server.sin_addr.s_addr = htonl(temp->ip);//htonl(temp->ip);

  			/*connect to the client-node*/
			if (connect(sock, serverptr, sizeof(server)) < 0)
			{
				fprintf(stderr,"Error in connect in user_on.\n");
				close(sock);
				return -1;				
			}

			strcpy(buf,"USER_ON");
			/*send user_on*/
			bytes = write(sock,buf,strlen(buf)+1);
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write in user_on.\n");
				close(sock);
				return -1;	
			}
			/*convert port and ip of the new client*/
			ip = htonl(client_ip);
			port = htons(client_port);
			/*send ip*/
			bytes = write(sock,&ip,sizeof(ip));
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write in user_on.\n");
				close(sock);
				return -1;	
			}
			/*send port*/
			bytes = write(sock,&port,sizeof(port));
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write in user_on.\n");
				close(sock);
				return -1;	
			}

			close(sock);	/*close the connection*/		
		}

		temp = temp->next_node; /*go to next client*/
	}

	return 0;
}

/*function to service the request log_on*/
int log_on(client_list** clients,int socket)
{	
	int insert = 0;
	uint32_t client_ip=0;
	/*read the new client's ip*/
	int bytes = read(socket,&client_ip,sizeof(client_ip));
	if (bytes < 0)
	{	
		fprintf(stderr,"Error in read in log_on.\n");
		return -1;
	}

	uint16_t client_port;
	/*read the new client's port*/
	bytes = read(socket,&client_port,sizeof(client_port));
	if (bytes < 0)
	{	
		fprintf(stderr,"Error in read in log_on.\n");
		return -1;
	}
 	/*convert port */
	client_port = ntohs(client_port);
	/*take the hostname of the new client*/
    struct hostent* h = gethostbyaddr(&client_ip,sizeof(client_ip),AF_INET);
	printf("LOG_ON:\t %s %d\n",h->h_name,client_port );

	/*convert ip*/
	client_ip = ntohl(client_ip);
	insert = insert_node(clients,client_port, client_ip); /*insert client in the clients list*/
	if (insert == 1)
		/*inform the other clients that this client is user_on*/
		if (user_on(*clients,client_port,client_ip) < 0)
			return -1;	

	return 0;

}

/*function to service the get_clients request*/
int get_clients(client_list** clients,int socket)
{
	char buf[32];
	int bytes;

	int nodes = counter_nodes(*clients);
	sprintf(buf,"CLIENT_LIST %d",nodes);

	printf("GET_CLIENTS\n");
	/*send CLIENT LIST 'number of clients'*/
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
	/*send every client of the client_list*/
	while(temp != NULL)
	{	
		/*convert ip and send it*/
		ip = htonl(temp->ip);
		bytes = write(socket, &ip , sizeof(ip) );
		if (bytes < 0)
		{
			fprintf(stderr,"Error in write in get_clients.\n");
			return -1 ;
		}

		/*convert port and send it*/
		port = htons(temp->port);
		bytes = write(socket,&port, sizeof(port)) ;
		if (bytes < 0)
		{
			fprintf(stderr,"Error in write in get_clients.\n");
			return -1;
		}
		temp = temp->next_node;	 /*go to next client of the list*/	
	}
	return 0;
}

/*function that informs the others clients that one client is off*/
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
	/*traverse all the client-list*/
	while(temp != NULL)
	{	/*if the client-node is not the client that exited now send the user_off*/
		if( (temp->ip != client_ip) || (temp->port != client_port) )
		{	
			/*make a new socket*/
			if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
			{
				fprintf(stderr,"Error in socket in user_off.\n");
				close(sock);
				return -1;
			}
			/*set the struct */
			bzero(&server, sizeof(server));
    		server.sin_family = AF_INET;       /* Internet domain */
    		server.sin_port = htons(temp->port); /* Server port */
  			server.sin_addr.s_addr = htonl(temp->ip);

  			/*make a connection with the client-node*/
			if (connect(sock, serverptr, sizeof(server)) < 0)
			{
				fprintf(stderr,"Error in connect in user_off.\n");
				close(sock);
				return -1;				
			}

			strcpy(buf,"USER_OFF");
			/*send the user_off*/
			bytes = write(sock,buf,strlen(buf)+1);
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write in user_off.\n");
				close(sock);
				return -1;	
			}
			/*convert ip and port and send them*/
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

			close(sock); /*close the connection*/	
		}

		temp = temp->next_node; /*fo to the next client*/
	}

	return 0;
}

/*function to service the request log_off*/
int log_off(client_list** clients,int socket)
{
	int del = 0;
	uint32_t client_ip=0;
	/*read the new client's ip*/
	int bytes = read(socket,&client_ip,sizeof(client_ip));
	if (bytes < 0)
	{	
		fprintf(stderr,"Error in read in log_off.\n");
		return -1;
	}
		
	uint16_t client_port;
	/*read the new client's port*/
	bytes = read(socket,&client_port,sizeof(client_port));
	if (bytes < 0)
	{	
		fprintf(stderr,"Error in read in log_off.\n");
		return -1;
	}
	client_port = ntohs(client_port); /*convert port*/

	/*print the client that exited*/
    struct hostent* h = gethostbyaddr(&client_ip,sizeof(client_ip),AF_INET);
	printf("LOG_OFF:\t %s %d\n",h->h_name,client_port );

	client_ip = ntohl(client_ip); 

	/*delete the client from the client_list*/
	del = delete_node(clients,client_port, client_ip);
	/*if the delete succeded  the client was surely in the list*/
	/* so send user_off to all the other clients of the list*/
	if (del == 1) 
	{	if (user_off(*clients,client_port,client_ip) < 0)
			return -1;
	}
	else
	{	/*if the delete does not succeded there is an error*/
		printf("ERROR_IP_PORT_NOT_FOUND_IN_LIST\n");
		return -2;
	}	

	return 0;
}
