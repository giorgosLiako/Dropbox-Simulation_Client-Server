

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

#include <pthread.h>

#include <signal.h>
#include <errno.h>

extern int errno;

#include <fcntl.h>

#include "list_interface.h"
#include "client_functions.h"
#include "buffer_interface.h"

#define CONNECTIONS 10
#define THREAD_NUMBER 100

int terminate = 0;

void sigint_handler(int sig)
{
	terminate = 1;
}

int service_request(int ,client_list**,char*);

client_list * clients ;
buffer  shared_buffer ;
pthread_mutex_t mtx;
pthread_mutex_t list_mtx;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;

static void cleanup_handler(void* arg)
{
	pthread_mutex_unlock(&mtx);
}

void* worker(void* ptr)
{
	pthread_cleanup_push(cleanup_handler,NULL);
	while(terminate == 0)
	{
		buffer_item* item = NULL;
		item = obtain(&shared_buffer);
		pthread_cond_signal(&cond_nonfull);
		if ( ( !strcmp(item->pathname,"\0")) && (item->version == 0))
		{
			send_get_file_list(item);
		}
		else
		{
			printf("NEXT STEP:\n");
			//send_get_file();
		}
		free(item);
	}
	pthread_cleanup_pop(0);
	return NULL;
}

int main(int argc , char* argv[])
{
	if (argc < 13)
	{
		fprintf(stderr,"Error: 13 arguments expected , %d given.\n",argc);
		return -1;
	}

	uint16_t 		port;
    int             listening_sock;
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
    gethostname(buf, sizeof(buf)); 
    host_entry = gethostbyname(buf); 
    IPstring = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); 

    uint32_t ip = inet_addr(IPstring);

	if ( log_on(serverptr,IPstring,ip,port) < 0)
	{
   		free(dirname);
		free(server_ip);
		return -3;		
	}

	if ( get_clients(serverptr,IPstring,ip,port,&clients) < 0)
	{
   		free(dirname);
		free(server_ip);
		return -3;		
	}

	initialize_buffer(&shared_buffer,buffer_size);
	pthread_mutex_init(&mtx, 0);
	pthread_mutex_init(&list_mtx, 0);
	pthread_cond_init(&cond_nonempty, 0);
	pthread_cond_init(&cond_nonfull, 0);

	pthread_mutex_lock(&list_mtx);
	client_list* temp = clients;
	while(temp != NULL)
	{
		buffer_item * item = (buffer_item*) malloc(sizeof(buffer_item));
		if (item == NULL)
		{
			fprintf(stderr, "Error in malloc in dropbox_client.c\n");
			free(dirname);
			free(server_ip);
			return -2;
		}

		item->ip = temp->ip;
		item->port = temp->port;
		item->pathname[0] = '\0';
		item->version = 0;
		place(&shared_buffer , item) ;
		pthread_cond_signal(&cond_nonempty);

		temp = temp->next_node;
	}
	pthread_mutex_unlock(&list_mtx);

	pthread_t *tids;
	if (worker_threads > THREAD_NUMBER) 
	{ /* Avoid too many threads */
		printf("Number of threads should be up to 100\n"); 
   		free(dirname);
		free(server_ip);
		return -4;	
	}

	if ((tids = (pthread_t*) malloc(worker_threads * sizeof(pthread_t))) == NULL) 
	{
		fprintf(stderr,"Error in malloc in dropbox_client.c \n");
		return -5;
	}
	printf("THREAD_CREATION\n");
	int err;
	for (int i=0 ; i < worker_threads ; i++)
	{	err = pthread_create(tids+i, NULL, worker, NULL);
		if (err != 0) 
		{
			fprintf(stderr, "Error in pthread_create in dropbox_client.c\n");
			free(dirname);
			free(server_ip);
			return -6;	
		} 
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
        			if (service_request(fd,&clients,dirname) <= 0)
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

	if ( log_off(serverptr,IPstring,ip,port) < 0)
	{
    	destroy_list(&clients);
    	destroy_buffer(&shared_buffer);
    	close(listening_sock);
		free(dirname);
		free(server_ip);
		free(tids);
		return -3;		
	}    


	for (int i = 0 ; i < worker_threads ; i++)
	{	
		int l = pthread_cancel(*(tids+i));
		if (l == 0) 
			printf("Thread %d cancelled\n",i);	
		
		err = pthread_join(*(tids+i), NULL);
		if (err != 0)
		{
			fprintf(stderr,"Error in pthread_join in dropbox_client.c\n");
			break;
		}
	}
	pthread_cond_destroy(&cond_nonempty);
	pthread_cond_destroy(&cond_nonfull);
	pthread_mutex_destroy(&mtx);
	pthread_mutex_destroy(&list_mtx);
    
    destroy_list(&clients);
    destroy_buffer(&shared_buffer);
    close(listening_sock);
	free(dirname);
	free(server_ip);
	free(tids);
	return 0;
}


int service_request(int socket ,client_list** clients,char* dir)
{
	char request[16];
	int i = 0;
	int read_bytes = 0;
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
	printf("%s\n",request);
	if ( !strcmp(request,"USER_ON"))
	{
		if (user_on(socket,clients,&shared_buffer) < 0)
			return -1;
	}
	else if ( !strcmp(request,"GET_FILE_LIST"))
	{
		if (responde_get_file_list(socket,dir) < 0)
			return -1;		
	}
	else if ( !strcmp(request,"RESPONDE_GET_FILE"))
	{
		//if (get_file(socket,clients) < 0)
		//	return -1;	
	}
	else if ( !strcmp(request,"USER_OFF"))
	{
		if (user_off(socket,clients) < 0)
			return -1;
	}
	else
		printf("Invalid request.\n");

	return 0;
}