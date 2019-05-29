

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
#include "server_functions.h"

#define CONNECTIONS 10

int terminate = 0;

void sigint_handler(int sig)
{
	terminate = 1;
}

void sigpipe_handler(int sig)
{
	printf("SIGPIPE\n");
}

int service_request(int ,client_list**);

int main(int argc , char* argv[])
{
	if (argc < 3)
	{
		fprintf(stderr,"Error: 3 arguments expected , %d given.\n",argc);
		return -1;
	}

	int             port, sock;
	struct sockaddr_in server;
	struct sockaddr *serverptr=(struct sockaddr *)&server;

	signal(SIGINT, sigint_handler);
	signal(SIGPIPE,sigpipe_handler);

	if ( !strcmp(argv[1],"-p") )
		port = atoi(argv[2]);

	char *IPbuffer; 
    struct hostent *host_entry; 
    char hostbuffer[256];
    gethostname(hostbuffer, sizeof(hostbuffer)); 
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
    	ready = select(fd_max+1, &read_set, NULL, NULL, NULL) ;
      	if ( (ready < 0) && (errno != EINTR) )
        {
        	fprintf(stderr,"Error in select\n");
          	break;
        }
        else if (ready <= 0) 
        	continue;
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
        			if (service_request(fd,&clients) <= 0)
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


int service_request(int socket, client_list ** clients)
{
	char request[16];
	int i = 0;
	int read_bytes = 0;
	char ch = 'a';

	while(ch != '\0')
	{	read_bytes = read(socket,request + i ,1);
		if (read_bytes < 0)
		{	
			fprintf(stderr,"Error in read in service_request.\n");
			return -2;
		}
		if (read_bytes == 0)
			return -1;
		ch = request[i];
		i++;
	}
	printf("%s\n",request);

	if (!strcmp(request,"LOG_ON"))
	{	
		if (log_on(clients,socket) < 0)
			return -1;
	}
	else if (!strcmp(request,"GET_CLIENTS"))
	{
		if (get_clients(clients,socket) < 0)
			return -1;
	}
	else if (!strcmp(request,"LOG_OFF"))
	{
		if( log_off(clients,socket) < 0)
			return -1;
	}
	else
		printf("Invalid request.\n");

	printf("End of request\n");
	return 0;
}

