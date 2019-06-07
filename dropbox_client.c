

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <stdint.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

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

#define CONNECTIONS 30
#define THREAD_NUMBER 100

int terminate = 0;

/*function to handle the signal SIGINT*/
void sigint_handler(int sig)
{
	terminate = 1;
}

int service_request(int ,client_list**,char*);

/*global variables*/
char mirror[128];
client_list * clients ;
buffer  shared_buffer ;
pthread_mutex_t buffer_mtx;
pthread_mutex_t list_mtx;
pthread_mutex_t str_mtx;
pthread_mutex_t print_mtx;

pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;

/*function to unblock a thread if it must cancel and it is blocked in a condition variable*/
static void cleanup_handler(void* arg)
{
	pthread_mutex_unlock(&buffer_mtx);
}

/*thread function*/
void* worker(void* ptr)
{
	pthread_cleanup_push(cleanup_handler,NULL);/*to work the cancel properly*/
	while(terminate == 0)/*run until cancel thread is asked*/
	{
		buffer_item* item = NULL;
		item = obtain(&shared_buffer); /*obtain a buffer_item from buffer*/
		pthread_cond_signal(&cond_nonfull); /*signal one thread that the buffer is not full*/

		/*if the buffer_item has no pathname and version */
		if ( ( !strcmp(item->pathname,"\0")) && (item->version == 0))
		{	/*send a get_file_list request*/
			send_get_file_list(item,mirror);
		}
		else
		{	/*send a get_file request*/
			send_get_file(item,clients,mirror);
		}
		free(item);/*free the obtained item */
	}
	pthread_cleanup_pop(0);/*to work the cancel properly*/
	return NULL;
}

int main(int argc , char* argv[])
{
	/*13 arguments should be provided */
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

	/*take the informations from the arguments*/
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
			strcpy(dirname,argv[i+1]); /*dirname*/
			i++;
		}
		else if ( !strcmp(argv[i],"-p"))
		{
			port = atoi(argv[i+1]); /*client's port */
		}
		else if ( !strcmp(argv[i],"-w"))
		{
			worker_threads = atoi(argv[i+1]); /*number of threads */
		}
		else if ( !strcmp(argv[i],"-b"))
		{
			buffer_size = atoi(argv[i+1]); /*buffer size */
		}
		else if ( !strcmp(argv[i],"-sp"))
		{
			server_port = atoi(argv[i+1]); /*server port */
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
			strcpy(server_ip,argv[i+1]); /*server ip*/
		}
	}

///////////////////////////////////////////////////////////////////////////
	/*make a socket */
    if ((listening_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
    	fprintf(stderr,"Error in socket\n");
    	return -2;
    }
    /*set the struct*/
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

	/*listen connections from server and from other clients to this socket*/
    if (listen(listening_sock, CONNECTIONS) < 0) 
    {
    	fprintf(stderr,"Error in listen\n");
    	close(listening_sock);
    	return -4;
    }
    
////////////////////////////////////////////////////////////////////////////

    /*set the server struct*/
   	bzero(&server, sizeof(server));
    server.sin_family = AF_INET;       /* Internet domain */
    
    server.sin_port = htons(server_port);         /* Server port */
  	
  	server.sin_addr.s_addr = inet_addr(server_ip);

    char * IPstring;
    struct hostent *host_entry; 
    gethostname(buf, sizeof(buf)); 

    /*get the ip of this client in printable form a.b.c.d*/
    host_entry = gethostbyname(buf); 
    IPstring = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); 

    uint32_t ip = inet_addr(IPstring);
    /*get the host name of this client*/
    struct hostent* h = gethostbyaddr(&ip,sizeof(ip),AF_INET);
    printf("PC_NAME : %s\n",h->h_name);


    /*make the directory that will have all the files of the other clients*/
    /* mirror directory = ./mirror_hostname_port */
    sprintf(mirror,"./mirror_%s_%d",h->h_name,port);
	if (mkdir(mirror, 0777)!= 0)
		printf("Mirror directory already exists.\n");
	else
		printf("Mirror Directory %s created.\n",mirror );     

	/*send a log_on request to server*/
	if ( log_on(serverptr,IPstring,ip,port) < 0)
	{
   		free(dirname);
		free(server_ip);
		return -3;		
	}

	/*send a get_clients request to server */
	if ( get_clients(serverptr,IPstring,ip,port,&clients,mirror) < 0)
	{
   		free(dirname);
		free(server_ip);
		return -3;		
	}
	/*print the client list */
	printf("GET CLIENTS:\n");
	print_list(clients);

	/*initialization of mutexes and condition variables */
	initialize_buffer(&shared_buffer,buffer_size);
	pthread_mutex_init(&buffer_mtx, 0);
	pthread_mutex_init(&list_mtx, 0);
	pthread_mutex_init(&str_mtx, 0);
	pthread_mutex_init(&print_mtx, 0);
	pthread_cond_init(&cond_nonempty, 0);
	pthread_cond_init(&cond_nonfull, 0);

	/*lock the list*/
	pthread_mutex_lock(&list_mtx);
	client_list* temp = clients;
	while(temp != NULL)
	{	/*for every client in the client list put a buffer_item in buffer*/
		buffer_item * item = (buffer_item*) malloc(sizeof(buffer_item));
		if (item == NULL)
		{
			fprintf(stderr, "Error in malloc in dropbox_client.c\n");
			free(dirname);
			free(server_ip);
			return -2;
		}
		/*buffer_item will be in form <ip,port,null pathname,version=0>*/
		item->ip = temp->ip;
		item->port = temp->port;
		item->pathname[0] = '\0';
		item->version = 0;
		place(&shared_buffer , item) ; /*place the item to the buffer*/
		pthread_cond_signal(&cond_nonempty); /*signal that the buffer is not empty*/

		temp = temp->next_node; /*go to next client*/
	}
	/*unlock the list*/
	pthread_mutex_unlock(&list_mtx);

	pthread_t *tids;
	if (worker_threads > THREAD_NUMBER) 
	{ /* Avoid too many threads */
		printf("Number of threads should be up to 100\n"); 
   		free(dirname);
		free(server_ip);
		return -4;	
	}
	/*malloc an array that will have the threads id*/
	if ((tids = (pthread_t*) malloc(worker_threads * sizeof(pthread_t))) == NULL) 
	{
		fprintf(stderr,"Error in malloc in dropbox_client.c \n");
		return -5;
	}
	/*create the threads */
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
	/*always have in variable fd_max the max file descriptor*/
    int fd_max = 0;
    fd_set set , read_set;
    if (listening_sock > fd_max) 
    	fd_max = listening_sock;

    FD_ZERO(&set); /*initialize the set*/
    FD_SET(listening_sock , &set); /*put listening sock in set*/

    while(terminate == 0)
    {
    	read_set = set;

    	int ready = 0;
    	/*select connections from the read_set when a file descriptor is ready*/
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
        	if (FD_ISSET(fd,&read_set)) /*if fd is ready*/
        	{
        		if (fd == listening_sock ) /*check if fd is the listening sock, so make a new connection*/
        		{	/*accept the new connection*/
        			int fd_client = accept(listening_sock, NULL, NULL);
        			FD_SET(fd_client,&set);
        			if (fd_client > fd_max)
        			{
        				fd_max = fd_client;
        			}
        			/*make unblock the connection with options*/
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
        		{	/*if fd is not the listening socket , service the request that came from another client or the server*/
        			if (service_request(fd,&clients,dirname) <= 0)
        			{
        				FD_CLR(fd,&set); /*remove fd from the set*/
        				if (fd == fd_max)
        					fd_max--;
        				close(fd); /*close the connection*/
        			}
        		}
        	}
        }
    }
    /*the client has received SIGINT signal so send a log_off request to server*/
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
	/*cancel all the creatd threads*/
	int cancelled_threads = 0;
	for (int i = 0 ; i < worker_threads ; i++)
	{	
		int l = pthread_cancel(*(tids+i)); /*cancel the thread*/
		if (l == 0) 
			cancelled_threads++;	
		
		err = pthread_join(*(tids+i), NULL); /*and join to the main thread*/
		if (err != 0)
		{
			fprintf(stderr,"Error in pthread_join in dropbox_client.c\n");
			break;
		}
	}

	if (cancelled_threads == worker_threads)
		printf("All threads have been canceled.\n");

	/*clean the memory*/
	pthread_cond_destroy(&cond_nonempty);
	pthread_cond_destroy(&cond_nonfull);
	pthread_mutex_destroy(&buffer_mtx);
	pthread_mutex_destroy(&list_mtx);
	pthread_mutex_destroy(&str_mtx);
	pthread_mutex_destroy(&print_mtx);

    destroy_list(&clients);
    destroy_buffer(&shared_buffer);
    close(listening_sock);
	free(dirname);
	free(server_ip);
	free(tids);

	return 0;
}

/*function to serve a request from another client or from server */
int service_request(int socket ,client_list** clients,char* dir)
{
	char request[16];
	int i = 0;
	int read_bytes = 0;
	char ch = 'z';

	/*read the request*/
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
	{	/*service user on request which is received from server */
		if (user_on(socket,clients,mirror) < 0)
			return -1;
	}
	else if ( !strcmp(request,"GET_FILE_LIST"))
	{	/*service get file list request which is received from another client */
		if (responde_get_file_list(socket,dir) < 0)
			return -1;		
	}
	else if ( !strcmp(request,"GET_FILE"))
	{	/*servise get file request which is received from another client */
		if (responde_get_file(socket,clients,dir) < 0)
			return -1;	
	}
	else if ( !strcmp(request,"USER_OFF"))
	{	/*servise user off request which is received from server */
		if (user_off(socket,clients) < 0)
			return -1;
	}
	else
		printf("Invalid request.\n");

	return 0;
}