

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

#include "list_interface.h"

#define CONNECTIONS 10

int terminate = 0;

void sigint_handler(int sig)
{
	terminate = 1;
}

void new_connection(int , int*);
int service_request(int ,client_list**);

int main(int argc , char* argv[])
{
	if (argc < 3)
	{
		fprintf(stderr,"Error: 3 arguments expected , %d given.\n",argc);
		return -1;
	}

	int             port, sock, newsock;
	struct sockaddr_in server, client;
	socklen_t clientlen;
	struct sockaddr *serverptr=(struct sockaddr *)&server;
	struct sockaddr *clientptr=(struct sockaddr *)&client;
	struct hostent *rem;

	signal(SIGINT, sigint_handler);

	if ( !strcmp(argv[1],"-p") )
		port = atoi(argv[2]);

	char *IPbuffer; 
    struct hostent *host_entry; 
    char hostbuffer[256];
    int hostname = gethostname(hostbuffer, sizeof(hostbuffer)); 
    host_entry = gethostbyname(hostbuffer); 
    IPbuffer = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); 

	printf("Server IP: %s\nServer port: %d\n",IPbuffer ,port);

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
    	fprintf(stderr,"Error in socket\n");
    	return -2;
    }

	server.sin_family = AF_INET;       /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);      /* The given port */
	

    /* Bind socket to address */
    if (bind(sock, serverptr, sizeof(server)) < 0)
    {
    	fprintf(stderr,"Error in bind\n");
    	close(sock);
    	return -3;
	}

    if (listen(sock, CONNECTIONS) < 0) 
    {
    	fprintf(stderr,"Error in listen\n");
    	close(sock);
    	return -4;
    }
    
    printf("Listening for connections to port %d\n", port);

    //////////////////////////////////////////////////////////////
    client_list * clients = NULL;

    int fd_max = 0;
    fd_set set , read_set;
    if (sock > fd_max)
    	fd_max = sock;

    FD_ZERO(&set);
    FD_SET(sock , &set);

    while(terminate == 0)
    {
    	read_set = set;

    	int ready = 0;
      	if ( (ready = select(fd_max+1, &read_set, NULL, NULL, NULL) ) < 0)
        {
        	fprintf(stderr,"Error in select\n");
          	break;
        }
        for(int fd=0; fd <= fd_max ; fd++)
        {
        	if (FD_ISSET(fd,&read_set))
        	{
        		if (fd == sock )
        		{
        			int fd_client = accept(sock, NULL, NULL);
        			FD_SET(fd_client,&set);
        			if (fd_client > fd_max)
        			{
        				fd_max = fd_client;
        			}
        				
        			int opts = fcntl(fd,F_GETFL);
					if (opts < 0)
					{
						fprintf(stderr,"fcntl(F_GETFL)\n");
						return -3;
					}

					opts = (opts | O_NONBLOCK);
					if (fcntl(fd,F_SETFL,opts) < 0) 
					{
						fprintf(stderr,"fcntl(F_SETFL)\n");
						return -3 ;
					}
        		}
        		else
        		{	
        			if (service_request(fd,&clients) < 0)
        			{
        				FD_CLR(fd,&set);
        				if (fd == fd_max)
        					fd_max--;
        				close(fd);
        			}
        		}
        	}
        }
    }

    destroy_list(&clients);
	close(sock);

	return 0;
}

void send_client_details(client_list* clients,uint16_t client_port,uint32_t client_ip)
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

	while(temp != NULL)
	{	
		if( (temp->ip != client_ip) || (temp->port != client_port) )
		{	
			if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
			{
				fprintf(stderr,"Error in socket\n");
				return;
			}
				
			bzero(&server, sizeof(server));
    		server.sin_family = AF_INET;       /* Internet domain */
    		server.sin_port = htons(temp->port); /* Server port */
  			server.sin_addr.s_addr = temp->ip;//htonl(temp->ip);

			if (connect(sock, serverptr, sizeof(server)) < 0)
			{
				fprintf(stderr,"Error in connect\n");
				return;				
			}

			strcpy(buf,"USER_ON ");
			
			bytes = write(sock,buf,strlen(buf));
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write\n");
				return;	
			}

			ip = htonl(client_ip);
			port = htons(client_port);

			bytes = write(sock,&ip,sizeof(ip));
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write\n");
				return;	
			}

			bytes = write(sock,&port,sizeof(port));
			if (bytes < 0)
			{
				fprintf(stderr,"Error in write\n");
				return;	
			}

			close(sock);			
		}

		temp = temp->next_node;
	}
}

int service_request(int socket, client_list ** clients)
{
	char request[16];
	int i = 0;
	int read_bytes = 0;
	int insert = 0;
	char ch = '\0';

	while(ch != ' ')
	{	read_bytes = read(socket,request + i ,1);
		if (read_bytes < 0)
		{	
			fprintf(stderr,"Error in read\n");
			return -2;
		}
		if (read_bytes == 0)
			return -1;
		ch = request[i];
		i++;
	}
	request[i-1] = '\0';
	printf("%s\n",request);

	if (!strcmp(request,"LOG_ON"))
	{	
		uint32_t client_ip=0;
		int bytes = read(socket,&client_ip,sizeof(client_ip));

		
		printf("%d\n",client_ip );
		
		client_ip = ntohl(client_ip); 
		
		struct in_addr ip_addr;
    	ip_addr.s_addr = client_ip;
		

		uint16_t client_port;
		bytes = read(socket,&client_port,sizeof(client_port));

		client_port = ntohs(client_port);
		//printf("%d %d\n",client_ip, client_port );
		
		printf("%s %d\n",inet_ntoa(ip_addr),client_port );

		insert = insert_node(clients,client_port, client_ip);
		if (insert == 1)
			send_client_details(*clients,client_port,client_ip);

	}
	else if (!strcmp(request,"GET_CLIENTS"))
	{
		int counter = counter_nodes(*clients , port ,uint32_t ip )
	}
	else if (!strcmp(request,"LOG_OFF"))
	{

	}
	else
		printf("Invalid request.\n");

	printf("End of request\n");
	return 0;
}


void new_connection(int sock, int* connected_socks)
{
	int connection = accept(sock, NULL, NULL);
	printf("%d\n",connection );
	if (connection < 0) 
	{
		fprintf(stderr, "Error in accept\n");
		return;
	}

	int opts = fcntl(sock,F_GETFL);
	if (opts < 0)
	{
		fprintf(stderr,"fcntl(F_GETFL)\n");
		return;
	}

	opts = (opts | O_NONBLOCK);
	if (fcntl(sock,F_SETFL,opts) < 0) 
	{
		fprintf(stderr,"fcntl(F_SETFL)\n");
		return;
	}

	for (int num = 0; (num < CONNECTIONS) && (connection != -1); num ++)
	{	if (connected_socks[num] == 0) 
		{
			printf("Connection accepted:   FD=%d; Slot=%d\n",connection,num);
			connected_socks[num] = connection;
			connection = -1;
		}
	}
	
	if (connection != -1) 
	{
		/* No room left in the queue! */
		printf("\nNo room left for new client.\n");
		char buffer[256];
		strcpy(buffer,"Sorry, this server is too busy. Try again later!\r\n");
		write(connection,buffer,strlen(buffer)+1);
		close(connection);
	}
	return;
}