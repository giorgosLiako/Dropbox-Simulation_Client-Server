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
#include "buffer_interface.h"
#include "file_functions.h"

extern buffer shared_buffer ;
extern pthread_mutex_t list_mtx;
extern pthread_cond_t cond_nonempty;

int send_get_file_list(buffer_item* item )
{	
	char buf[64];
	int sock ;

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
   	{
   		fprintf(stderr,"Error in socket\n");
    	return -1;
   	}

    struct sockaddr_in  other_client;
    struct sockaddr *other_client_ptr = (struct sockaddr*)&other_client;

	bzero(&other_client, sizeof(other_client));
    other_client.sin_family = AF_INET;       /* Internet domain */
    other_client.sin_port = htons(item->port); /* other_client port */
  	other_client.sin_addr.s_addr = htonl(item->ip);//htonl(temp->ip);

    if (connect(sock, other_client_ptr, sizeof(struct sockaddr_in)) < 0)
    {
    	fprintf(stderr,"Error in connect\n");
    	close(sock);
    	return -1;
    }

	strcpy(buf,"GET_FILE_LIST");
			
	int bytes = write(sock,buf,strlen(buf)+1);
	if (bytes < 0)
	{
		fprintf(stderr,"Error in write in send_get_file_list.\n");
		close(sock);
		return -1;	
	}

	int i = 0 ;
	char ch = 'z';

	while(ch != '\0') 
	{	bytes = read(sock,buf + i ,1);
		if (bytes < 0)
		{	
			fprintf(stderr,"Error in read in service_request.\n");
			close(sock);
			return -2;
		}
		if (bytes == 0)
		{ printf("Nothing to read\n");	return -1;}
		ch = buf[i];
		i++;
	}
	printf("%s ",buf);
	char *token;
   
   	token = strtok(buf," ");

   	if (token != NULL)
   		token = strtok(NULL," ");
   	else
   	{
   		printf("Something went wrong, invalid message from server.\n");
   		close(sock);
   		return -3;
   	}

   	int N = atoi(token);

   	for (int i = 0 ; i < N ; i++)
   	{
   		int j = 0 ;
		ch = 'z';

		while(ch != '\0') 
		{	bytes = read(sock,buf + j ,1);
			if (bytes < 0)
			{	
				fprintf(stderr,"Error in read in send_get_file_list.\n");
				close(sock);
				return -2;
			}
			if (bytes == 0)
			{ printf("Nothing to read\n");	return -1;}
			ch = buf[j];
			j++;
		}
		int name_size = atoi(buf);

		bytes = read(sock,buf,name_size);
		if (bytes < 0)
		{
			fprintf(stderr, "Error in read in send_get_file_list. \n");
			return -2;
		}

		buffer_item * new_item = (buffer_item*) malloc(sizeof(buffer_item));
		if (new_item == NULL)
		{
			fprintf(stderr, "Error in malloc in send_get_file_list.\n" );
			return -3;
		}

		strcpy(new_item->pathname,buf);
		printf("PATH: %s \t",new_item->pathname);

   		j = 0 ;
		ch = 'z';

		while(ch != '\0') 
		{	bytes = read(sock,buf + j ,1);
			if (bytes < 0)
			{	
				fprintf(stderr,"Error in read in send_get_file_list.\n");
				close(sock);
				return -2;
			}
			if (bytes == 0)
			{ printf("Nothing to read\n");	return -1;}
			ch = buf[j];
			j++;
		}

		new_item->version = atoi(buf);
		printf(" Version: %ld\n",new_item->version );
		new_item->port = item->port;   	
		new_item->ip  = item->ip;
		place(&shared_buffer , new_item) ;
		pthread_cond_signal(&cond_nonempty);
	}

    close(sock);
	return 0 ;
}

int responde_get_file_list(int socket,char* dir)
{
	char buf[64];

	int number_of_files = files_counter(dir,NULL);

	sprintf(buf,"FILE_LIST %d",number_of_files);

	printf("%s\n",buf );

	write(socket,buf,strlen(buf)+1);

	int paths_sent = send_pathnames(dir,NULL,socket);

	if (paths_sent == number_of_files)
		printf("All files have been sent\n");
	else
		printf("ERROR: Number of files: %d \tSent files: %d\n",number_of_files,paths_sent );

	return 0;
}

int send_get_file(int socket , client_list** clients)
{
	return 0;
}

int get_clients(struct sockaddr *serverptr,char* IPstring,uint32_t ip,uint16_t port,client_list** clients)
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

    //uint32_t ip = inet_addr(IPstring);
    
    strcpy(buf,"GET_CLIENTS");

    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write.\n");
    	close(sock_request);
    	return -1;
    }

 	char request[32];
	int i = 0 ;
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
    	printf("IP: %d IP_CLIENT: %d , PORT: %d , PORT_CLIENT: %d\n" , ntohl(ip) , client_ip , port , client_port);
    	if (( ntohl(ip) != client_ip) || (port != client_port))
    	{	
    		pthread_mutex_lock(&list_mtx);
    		insert_node(clients,client_port,client_ip);
    		pthread_mutex_unlock(&list_mtx);
    	}
   	}
   	pthread_mutex_lock(&list_mtx); 
   	printf("client-nodes: %d\n",counter_nodes(*clients));
	pthread_mutex_unlock(&list_mtx);
	close(sock_request );

	return 0;	
}

int log_on(struct sockaddr *serverptr,char* IPstring,uint32_t ip,uint16_t port)
{
	printf("LOG_ON\n");
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
    //uint32_t ip = inet_addr(IPstring);

    strcpy(buf, "LOG_ON");

    int bytes =0;
    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_on.\n");
    	close(sock_request );
    	return -1 ;
    }

    printf("IP: %s port: %d\n",IPstring,port );

    uint32_t u_ip = ip;//htonl(ip);
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
	printf("END OF LOG_ON\n");
	return 0;
}

int log_off(struct sockaddr *serverptr,char* IPstring,uint32_t ip,uint16_t port)
{
	char buf[64];
	int sock_request;
	printf("LOG_OFF\n");
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
    // uint32_t ip = inet_addr(IPstring);

    strcpy(buf, "LOG_OFF");

    int bytes =0;
    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_off.\n");
    	close(sock_request );
    	return -1 ;
    }

    printf("IP: %s port: %d\n",IPstring,port );

    uint32_t u_ip = ip;//htonl(ip);
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
    printf("END OF LOG_OFF\n");
	close(sock_request ); 
	return 0;
}

int user_on(int socket,client_list** clients,buffer * shared_buffer)
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
    printf("%d\n",ip );

    pthread_mutex_lock(&list_mtx);
    
    if (search_list(*clients,port,ip) == NULL)
    {	
    	insert_node(clients,port,ip);
 		buffer_item * item = (buffer_item*) malloc(sizeof(buffer_item));
		if (item == NULL)
		{
			fprintf(stderr, "Error in malloc in user_on in client_functions.c\n");
			return -2;
		}

		item->ip = ip;
		item->port = port;
		item->pathname[0] = '\0';
		item->version = 0;
		place(shared_buffer , item) ;
		pthread_cond_signal(&cond_nonempty);

    }
    pthread_mutex_unlock(&list_mtx);

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

   	pthread_mutex_lock(&list_mtx);

	print_list(*clients);
    if (search_list(*clients,port,ip) != NULL)
    {	
    	delete_node(clients,port,ip);
    }
    else
    {	
    	printf("ERROR_IP_PORT_NOT_FOUND_IN_LIST\n");
	}
	print_list(*clients);

	pthread_mutex_unlock(&list_mtx);

    struct in_addr ip_addr;
    ip_addr.s_addr = ip;

    printf("USER_OFF %s %d\n",inet_ntoa(ip_addr),port);
    return 0;
}