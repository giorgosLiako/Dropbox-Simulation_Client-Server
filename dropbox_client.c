

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <stdint.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <signal.h>

#include <fcntl.h>

#include "list_interface.h"

#define CONNECTIONS 10


int terminate = 0;

void sigint_handler(int sig)
{
	terminate = 1;
}

int service_request(int ,client_list**);

int main(int argc , char* argv[])
{
	if (argc < 13)
	{
		fprintf(stderr,"Error: 13 arguments expected , %d given.\n",argc);
		return -1;
	}

    int             port, sock_request , i , listening_sock;
    char            buf[256];
    struct sockaddr_in server, client;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct sockaddr *clientptr=(struct sockaddr *)&client;

	char* dirname = NULL;
	char* server_ip = NULL;
	int worker_threads = 0;
	int buffer_size = 0;
	int server_port;

	for (int i = 0 ; i < argc ; i++)
	{
		if ( !strcmp(argv[i],"-d"))
		{
			dirname = (char*) malloc( (strlen(argv[i+1])+1) * sizeof(char));
			if (dirname == NULL)
			{
				fprintf(stderr,"Error in malloc.\n");
				return -2;
			}
			strcpy(dirname,argv[i+1]);
			i++;
		}
		else if ( !strcmp(argv[i],"-p"))
		{
			port = atoi(argv[i+1]);
		}
		else if ( !strcmp(argv[i],"-w"))
		{
			worker_threads = atoi(argv[i+1]);
		}
		else if ( !strcmp(argv[i],"-b"))
		{
			buffer_size = atoi(argv[i+1]);
		}
		else if ( !strcmp(argv[i],"-sp"))
		{
			server_port = atoi(argv[i+1]);
		}
		else if ( !strcmp(argv[i],"-sip"))
		{
			server_ip = (char*) malloc( (strlen(argv[i+1])+1) * sizeof(char));
			if (server_ip == NULL)
			{
				fprintf(stderr,"Error in malloc.\n");
				free(dirname);
				return -2;
			}
			strcpy(server_ip,argv[i+1]);
		}
	}

///////////////////////////////////////////////////////////////////////////

    if ((listening_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
    	fprintf(stderr,"Error in socket\n");
    	return -2;
    }

	client.sin_family = AF_INET;       /* Internet domain */
    client.sin_addr.s_addr = htonl(INADDR_ANY);
    client.sin_port = htons(port);      /* The given port */
	

    /* Bind socket to address */
    if (bind(listening_sock, clientptr, sizeof(client)) < 0)
    {
    	fprintf(stderr,"Error in bind\n");
    	close(listening_sock);
    	return -3;
	}

    if (listen(listening_sock, CONNECTIONS) < 0) 
    {
    	fprintf(stderr,"Error in listen\n");
    	close(listening_sock);
    	return -4;
    }
    
////////////////////////////////////////////////////////////////////////////

	if ((sock_request = socket(PF_INET, SOCK_STREAM, 0)) < 0)
   	{
   		fprintf(stderr,"Error in socket\n");
   		free(dirname);
		free(server_ip);
    	return -3;
   	}
   	bzero(&server, sizeof(server));
    server.sin_family = AF_INET;       /* Internet domain */
    
    server.sin_port = htons(server_port);         /* Server port */
  	
  	server.sin_addr.s_addr = inet_addr(server_ip);
 
    if (connect(sock_request, serverptr, sizeof(server)) < 0)
    {
    	fprintf(stderr,"Error in connect\n");
    	close(sock_request); 
    	free(dirname);
		free(server_ip);
    	return -4;
    }

    printf("Connecting to %s port %d\n", server_ip, port);

    char * IPstring;
    struct hostent *host_entry; 
    int hostname = gethostname(buf, sizeof(buf)); 
    host_entry = gethostbyname(buf); 
    IPstring = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); 
    
    uint32_t ip = inet_addr(IPstring);

    strcpy(buf, "LOG_ON ");

    int b =0;
    if ( (b = write(sock_request ,buf,strlen(buf)) ) < 0 )
    {
    	fprintf(stderr, "Error in write.\n");
    	return -5;
    }

    printf("IP: %s port: %d\n",IPstring,port );

    ip = htonl(ip);
    printf("%d\n",ip );

    if ( (b = write(sock_request , &ip, sizeof(ip)) ) < 0 )
    {
    	fprintf(stderr, "Error in write.\n");
    	return -5;
    }

    uint16_t u_port =  htons(port);


    if ( ( b = write(sock_request , &u_port, sizeof(u_port)) ) < 0 )
    {
    	fprintf(stderr, "Error in write.\n");
    	return -5;
    }
    printf("%d %d\n",ip,port );

	close(sock_request ); 

	if ((sock_request = socket(PF_INET, SOCK_STREAM, 0)) < 0)
   	{
   		fprintf(stderr,"Error in socket\n");
   		free(dirname);
		free(server_ip);
    	return -3;
   	}

    if (connect(sock_request, serverptr, sizeof(server)) < 0)
    {
    	fprintf(stderr,"Error in connect\n");
    	close(sock_request); 
    	free(dirname);
		free(server_ip);
    	return -4;
    }
    
    strcpy(buf,"GET_CLIENTS ");

    if ( (b = write(sock_request ,buf,strlen(buf)) ) < 0 )
    {
    	fprintf(stderr, "Error in write.\n");
    	return -5;
    }

	close(sock_request );
   
   client_list * clients = NULL;

    int fd_max = 0;
    fd_set set , read_set;
    if (listening_sock > fd_max)
    	fd_max = listening_sock;

    FD_ZERO(&set);
    FD_SET(listening_sock , &set);

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
        		if (fd == listening_sock )
        		{
        			int fd_client = accept(listening_sock, NULL, NULL);
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
    close(listening_sock);
	free(dirname);
	free(server_ip);
	return 0;
}

int service_request(int socket ,client_list** L)
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

	if ( !strcmp(request,"USER_ON"))
	{
		uint32_t ip;
    	uint16_t port;

    	read_bytes = read(socket,&ip,sizeof(ip));
    	if (read_bytes < 0)
    	{
			fprintf(stderr,"Error in read\n");
			return -2;    		
    	}

    	read_bytes = read(socket,&port,sizeof(port));
    	if (read_bytes < 0)
    	{
			fprintf(stderr,"Error in read\n");
			return -2;    		
    	}

    	ip = ntohl(ip);
    	port = ntohs(port);

    	struct in_addr ip_addr;
    	ip_addr.s_addr = ip;

    	printf("%s %s %d\n",request,inet_ntoa(ip_addr),port);

	}

	return 0;
}