

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
int log_on(struct sockaddr *,char *,uint16_t);
int log_off(struct sockaddr *,char* ,uint16_t );
int get_clients(struct sockaddr *,char*,uint16_t,client_list**);

int main(int argc , char* argv[])
{
	if (argc < 13)
	{
		fprintf(stderr,"Error: 13 arguments expected , %d given.\n",argc);
		return -1;
	}

	uint16_t 		port;
    int             sock_request , listening_sock;
    char            buf[256];
    struct sockaddr_in server, client;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct sockaddr *clientptr=(struct sockaddr *)&client;

	signal(SIGINT, sigint_handler);

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


   	bzero(&server, sizeof(server));
    server.sin_family = AF_INET;       /* Internet domain */
    
    server.sin_port = htons(server_port);         /* Server port */
  	
  	server.sin_addr.s_addr = inet_addr(server_ip);

    char * IPstring;
    struct hostent *host_entry; 
    int hostname = gethostname(buf, sizeof(buf)); 
    host_entry = gethostbyname(buf); 
    IPstring = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); 

	if ( log_on(serverptr,IPstring,port) < 0)
	{
   		free(dirname);
		free(server_ip);
		return -3;		
	}

   	client_list * clients = NULL;

	if ( get_clients(serverptr,IPstring,port,&clients) < 0)
	{
   		free(dirname);
		free(server_ip);
		return -3;		
	}

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
    	//ready = select(fd_max+1, &read_set, NULL, NULL, NULL);
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

	if ( log_off(serverptr,IPstring,port) < 0)
	{
    	destroy_list(&clients);
    	close(listening_sock);
		free(dirname);
		free(server_ip);
		return -3;		
	}    

    destroy_list(&clients);
    close(listening_sock);
	free(dirname);
	free(server_ip);
	return 0;
}

int get_clients(struct sockaddr *serverptr,char* IPstring,uint16_t port,client_list** clients)
{
	int bytes;
	int sock_request;
	char buf[64];

	if ((sock_request = socket(PF_INET, SOCK_STREAM, 0)) < 0)
   	{
   		fprintf(stderr,"Error in socket\n");
    	return -1;
   	}

    if (connect(sock_request, serverptr, sizeof(struct sockaddr_in)) < 0)
    {
    	fprintf(stderr,"Error in connect\n");
    	close(sock_request);
    	return -1;
    }

    uint32_t ip = inet_addr(IPstring);
    
    strcpy(buf,"GET_CLIENTS");

    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write.\n");
    	close(sock_request);
    	return -1;
    }

 	char request[32];
	int i = 0 , spaces = 0;
	char ch = 'z';

	while(ch != '\0') 
	{	bytes = read(sock_request,request + i ,1);
		if (bytes < 0)
		{	
			fprintf(stderr,"Error in read in service_request.\n");
			close(sock_request);
			return -2;
		}
		if (bytes == 0)
			return -1;
		ch = request[i];
		i++;
	}
	//request[i-1] = '\0';
	printf("REQUEST: %s\n",request );

	char *token;
   
   	token = strtok(request," ");

   	if (token != NULL)
   		token = strtok(NULL," ");
   	else
   	{
   		printf("Something went wrong, invalid message from server.\n");
   		close(sock_request);
   		return -3;
   	}


   	int N = atoi(token);
   	printf("CLIENTS: %d\n",N);

   	for(i = 0 ; i < N ; i++)
   	{
   		uint32_t client_ip;
    	uint16_t client_port;

    	bytes = read(sock_request,&client_ip,sizeof(client_ip));
    	if (bytes < 0)
    	{
			fprintf(stderr,"Error in read\n");
			close(sock_request);
			return -2;    		
    	}

    	bytes = read(sock_request,&client_port,sizeof(client_port));
    	if (bytes < 0)
    	{
			fprintf(stderr,"Error in read\n");
		    close(sock_request);
			return -2;    		
    	}

    	client_ip = ntohl(client_ip);
    	client_port = ntohs(client_port);

    	if ((ip != client_ip) || (port != client_port))
    		insert_node(clients,client_port,client_ip);

   	} 
   	printf("client-nodes: %d\n",counter_nodes(*clients));
	close(sock_request );

	return 0;	
}

int log_on(struct sockaddr *serverptr,char* IPstring,uint16_t port)
{
	char buf[64];
	int sock_request;

	if ((sock_request = socket(PF_INET, SOCK_STREAM, 0)) < 0)
   	{
   		fprintf(stderr,"Error in socket in log_on.\n");
    	return -1 ;
   	}

    if (connect(sock_request, serverptr, sizeof(struct sockaddr_in)) < 0)
    {
    	fprintf(stderr,"Error in connect in log_on.\n");
    	close(sock_request); 
    	return -1 ;
    }	

	//printf("Connecting to %s port %d\n", server_ip, port);
    uint32_t ip = inet_addr(IPstring);

    strcpy(buf, "LOG_ON");

    int bytes =0;
    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_on.\n");
    	close(sock_request );
    	return -1 ;
    }

    printf("IP: %s port: %d\n",IPstring,port );

    uint32_t u_ip = htonl(ip);
    printf("%d\n",u_ip );

    if ( (bytes = write(sock_request , &u_ip, sizeof(u_ip)) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_on.\n");
    	close(sock_request );
    	return -1 ;
    }

    uint16_t u_port =  htons(port);


    if ( ( bytes = write(sock_request , &u_port, sizeof(u_port)) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_on.\n");
    	close(sock_request );
    	return -1 ;
    }
    printf("%d %d\n",ip,port );

	close(sock_request ); 
	return 0;
}

int log_off(struct sockaddr *serverptr,char* IPstring,uint16_t port)
{
	char buf[64];
	int sock_request;
	printf("IN LOG_OFF\n");
	if ((sock_request = socket(PF_INET, SOCK_STREAM, 0)) < 0)
   	{
   		fprintf(stderr,"Error in socket in log_off.\n");
    	return -1 ;
   	}

    if (connect(sock_request, serverptr, sizeof(struct sockaddr_in)) < 0)
    {
    	fprintf(stderr,"Error in connect in log_off.\n");
    	close(sock_request); 
    	return -1 ;
    }	

	//printf("Connecting to %s port %d\n", server_ip, port);
    uint32_t ip = inet_addr(IPstring);

    strcpy(buf, "LOG_OFF");

    int bytes =0;
    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_off.\n");
    	close(sock_request );
    	return -1 ;
    }

    printf("IP: %s port: %d\n",IPstring,port );

    uint32_t u_ip = htonl(ip);
    printf("%d\n",u_ip );

    if ( (bytes = write(sock_request , &u_ip, sizeof(u_ip)) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_off.\n");
    	close(sock_request );
    	return -1 ;
    }

    uint16_t u_port =  htons(port);


    if ( ( bytes = write(sock_request , &u_port, sizeof(u_port)) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_off.\n");
    	close(sock_request );
    	return -1 ;
    }
    printf("%d %d\n",ip,port );

	close(sock_request ); 
	return 0;
}

int user_on(int socket,client_list** clients)
{
	uint32_t ip;
    uint16_t port;

    int read_bytes;

    read_bytes = read(socket,&ip,sizeof(ip));
    if (read_bytes < 0)
    {
		fprintf(stderr,"Error in read in user_on\n");
		return -2;    		
   	}

    read_bytes = read(socket,&port,sizeof(port));
    if (read_bytes < 0)
    {
		fprintf(stderr,"Error in read in user on\n");
		return -2;    		
   	}

   	ip = ntohl(ip);
    port = ntohs(port);

    if (search_list(*clients,port,ip) == NULL)
    	insert_node(clients,port,ip);

    struct in_addr ip_addr;
    ip_addr.s_addr = ip;

    printf("USER_ON %s %d\n",inet_ntoa(ip_addr),port);
    return 0;
}

int user_off(int socket,client_list** clients)
{
	uint32_t ip;
    uint16_t port;

    int read_bytes;

    read_bytes = read(socket,&ip,sizeof(ip));
    if (read_bytes < 0)
    {
		fprintf(stderr,"Error in read in user_on\n");
		return -2;    		
   	}

    read_bytes = read(socket,&port,sizeof(port));
    if (read_bytes < 0)
    {
		fprintf(stderr,"Error in read in user on\n");
		return -2;    		
   	}

   	ip = ntohl(ip);
    port = ntohs(port);

    if (search_list(*clients,port,ip) != NULL)
    	insert_node(clients,port,ip);
    else
    	printf("The user is not in the list\n");

    struct in_addr ip_addr;
    ip_addr.s_addr = ip;

    printf("USER_OFF %s %d\n",inet_ntoa(ip_addr),port);
    return 0;
}

int service_request(int socket ,client_list** clients)
{
	char request[16];
	int i = 0;
	int read_bytes = 0;
	int insert = 0;
	char ch = 'z';

	while(ch != '\0')
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

	if ( !strcmp(request,"USER_ON"))
	{
		if (user_on(socket,clients) < 0)
			return -1;
	}
	else if ( !strcmp(request,"USER_OFF"))
	{
		if (user_off(socket,clients) < 0)
			return -1;
	}

	return 0;
}