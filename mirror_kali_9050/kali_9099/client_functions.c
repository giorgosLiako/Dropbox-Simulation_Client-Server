#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <stdint.h>
#include <utime.h>

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
#include "buffer_interface.h"
#include "file_functions.h"

extern buffer shared_buffer ;
extern pthread_mutex_t list_mtx;
extern pthread_mutex_t str_mtx;
extern pthread_mutex_t print_mtx;
extern pthread_cond_t cond_nonempty;

int send_get_file_list(buffer_item* item ,char* mirror)
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

	pthread_mutex_lock(&str_mtx);
	strcpy(buf,"GET_FILE_LIST");
	pthread_mutex_unlock(&str_mtx);

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
	printf("%s \n",buf);
	
	char *token;

	pthread_mutex_lock(&str_mtx);   
   	token = strtok(buf," ");

   	if (token != NULL)
   		token = strtok(NULL," ");
   	else
   	{
   		printf("Something went wrong, invalid message from server.\n");
   		close(sock);
   		pthread_mutex_unlock(&str_mtx);
   		return -3;
   	}
   	pthread_mutex_unlock(&str_mtx);

   	int N = atoi(token);
   	int counter = 0;
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


		uint32_t mir_ip = other_client.sin_addr.s_addr;
		char path[128];
	    struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET);
    	
    	pthread_mutex_lock(&str_mtx);
    	sprintf(path,"%s/%s_%d/%s",mirror,h->h_name,item->port,buf);
    	
    	strcpy(new_item->pathname,buf);
    	pthread_mutex_unlock(&str_mtx);
		
		directory_maker(path);
  		
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

		new_item->port = item->port;   	
		new_item->ip  = item->ip;
		place(&shared_buffer , new_item) ;
		pthread_cond_signal(&cond_nonempty);
	
		counter++;
	}
	
	pthread_mutex_lock(&print_mtx);
	
	if (counter == N)
		printf("All the file list has been received.\n");
	else
		printf("ERROR: Number of files in file list: %d \tReceived files of file list: %d\n",N,counter );

	pthread_mutex_unlock(&print_mtx);

    close(sock);
	return 0 ;
}

int responde_get_file_list(int socket,char* dir)
{
	char buf[64];

	int number_of_files = files_counter(dir,NULL);

	sprintf(buf,"FILE_LIST %d",number_of_files);

	write(socket,buf,strlen(buf)+1);

	int paths_sent = send_pathnames(dir,NULL,socket);

	pthread_mutex_lock(&print_mtx);
	if (paths_sent == number_of_files)
		printf("All the file list has been sent\n");
	else
		printf("ERROR: Number of files in file_list: %d \tSent files of file list: %d\n",number_of_files,paths_sent );
	pthread_mutex_unlock(&print_mtx);
	return 0;
}

int send_get_file(buffer_item* item, client_list* clients,char* mirror)
{
	if (search_list(clients,item->port,item->ip) == NULL)
	{
		printf("Client is not in the list.\n");
		return -1;
	} 

	uint32_t mir_ip = htonl(item->ip);
	char path[256];
	struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET);
    	
    pthread_mutex_lock(&str_mtx);
    sprintf(path,"%s/%s_%d/%s",mirror,h->h_name,item->port,item->pathname);
    pthread_mutex_unlock(&str_mtx);
	
	int exist = 0;
	struct stat  st;   
  	if (stat (path, &st) != 0)
  		exist = 0;
  	else
  		exist = 1;

	char buf[256];
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
 
	int bytes;

    pthread_mutex_lock(&str_mtx);	
	strcpy(buf,"GET_FILE");
	pthread_mutex_unlock(&str_mtx);

	bytes = write(sock,buf,strlen(buf)+1);
	if (bytes < 0)
	{
		fprintf(stderr,"Error in write in send_get_file.\n");
		return -3;
	}
	
	if (exist == 0)
	{
		pthread_mutex_lock(&str_mtx);
		sprintf(buf,"%s 0",item->pathname);
		pthread_mutex_unlock(&str_mtx);

		bytes = write(sock,buf,strlen(buf)+1);
		if (bytes < 0)
		{
			fprintf(stderr,"Error in write in send_get_file.\n");
			return -3;
		}
	}
	else
	{	pthread_mutex_lock(&str_mtx);
		sprintf(buf,"%s %ld",item->pathname,st.st_mtim.tv_sec);
		pthread_mutex_unlock(&str_mtx);

		bytes = write(sock,buf,strlen(buf)+1);
		if (bytes < 0)
		{
			fprintf(stderr,"Error in write in send_get_file.\n");
			return -3;
		}
		
	}
	
	int i = 0 ;
	char ch = 'z';

	while(ch != '\0') 
	{	bytes = read(sock,buf + i ,1);
		if (bytes < 0)
		{	
			fprintf(stderr,"Error in read in send_get_file.\n");
			close(sock);
			return -2;
		}
		if (bytes == 0)
			return -1;
		ch = buf[i];
		i++;		
	}

	if ( !strcmp(buf,"FILE_NOT_FOUND"))
	{
		pthread_mutex_lock(&print_mtx);
		printf("FILE %s NOT FOUND\n",path);
		pthread_mutex_unlock(&print_mtx);
	}
	else if (!strcmp(buf,"FILE_SIZE"))
	{
		i = 0 ;
		ch = 'z';

		while(ch != '\0') 
		{	bytes = read(sock,buf + i ,1);
			if (bytes < 0)
			{	
				fprintf(stderr,"Error in read in send_get_file.\n");
				close(sock);
				return -2;
			}
			if (bytes == 0)
				return -1;
			ch = buf[i];
			i++;		
		}

		pthread_mutex_lock(&str_mtx);
		char *token;
   
   		token = strtok(buf," ");
   		time_t version = (time_t) atoi(token);
   		int n ;
   		if (token != NULL)
   		{	token = strtok(NULL," ");
   			n = atoi(token);
   		}
   		pthread_mutex_unlock(&str_mtx);

   		int fd = open(path,O_CREAT | O_WRONLY , 0777);
   		if (fd < 0)
   		{
   			fprintf(stderr,"Error in open in send_get_file.");
   			return -3;
   		}

   		int count = 0;
   		for(int j = 0 ; j < n ; j++)
   		{
   			read(sock,buf,1);
   			write(fd,buf,1);
   			count++;
   		}

   		pthread_mutex_lock(&print_mtx);
   		if (count == n)
   			printf("File %s received successfully\n",path );
   		pthread_mutex_unlock(&print_mtx);

   		struct utimbuf ut;
   		ut.actime = version;
   		ut.modtime = version;
   		utime(path, &ut);
	}
	else if (!strcmp(buf,"FILE_UP_TO_DATE"))
	{	
		pthread_mutex_lock(&print_mtx);
		printf("FILE %s UP TO DATE\n",path );
		pthread_mutex_unlock(&print_mtx);
	}

	return 0;
}

int responde_get_file(int sock,client_list ** clients,char* dir)
{
	char buf[128];
	char pathname[128];
	int i = 0 ,bytes;
	char ch = 'z';

	while(ch != '\0') 
	{	bytes = read(sock,buf + i ,1);
		if (bytes < 0)
		{	
			fprintf(stderr,"Error in read in responde_get_file.\n");
			close(sock);
			return -2;
		}
		if (bytes == 0)
			return -1;
		ch = buf[i];
		i++;		
	}

	pthread_mutex_lock(&str_mtx);
	char* token;

	token = strtok(buf," ");
	time_t version;
	if (token != NULL)
	{	strcpy(pathname,token);
		token = strtok(NULL," ");
		if (token != NULL)
			version = (time_t) atoi(token);
	}

	strcpy(buf,dir);
	strcat(buf,"/");
	strcat(buf,pathname);

	strcpy(pathname,buf);
	pthread_mutex_unlock(&str_mtx);

	struct stat  st;   
  	if (stat (pathname, &st) != 0)
  	{	
  
  		strcpy(buf,"FILE_NOT_FOUND");
  		bytes = write(sock,buf,strlen(buf)+1);
  		if (bytes < 0 )
  		{
  			fprintf(stderr,"Error in write in responde_get_file.");
  			return -1;
  		}
  		return 0;
  	}

  	if ( st.st_mtim.tv_sec == version)
  	{
  		strcpy(buf,"FILE_UP_TO_DATE");
  		bytes = write(sock,buf,strlen(buf)+1);
  		if (bytes < 0 )
  		{
  			fprintf(stderr,"Error in write in responde_get_file.");
  			return -1;
  		}
  		return 0;  		
  	}
  	else if ( st.st_mtim.tv_sec > version )
  	{
  		strcpy(buf,"FILE_SIZE");
   		bytes = write(sock,buf,strlen(buf)+1);
  		if (bytes < 0 )
  		{
  			fprintf(stderr,"Error in write in responde_get_file.");
  			return -1;
  		}

  		sprintf(buf,"%ld %ld",st.st_mtim.tv_sec,st.st_size);
   		bytes = write(sock,buf,strlen(buf)+1);
  		if (bytes < 0 )
  		{
  			fprintf(stderr,"Error in write in responde_get_file.");
  			return -1;
  		}

   		int fd = open(pathname,O_RDONLY);
   		if (fd < 0)
   		{
   			fprintf(stderr,"Error in open in send_get_file.");
   			return -3;
   		}

   		int count = 0;
   		for(int j = 0 ; j < st.st_size ; j++)
   		{
   			read(fd,buf,1);
   			write(sock,buf,1);
   			count++;
   		}

   		pthread_mutex_lock(&print_mtx);
   		if (count == st.st_size)
   			printf("File %s sent successfully\n",pathname );
   		pthread_mutex_unlock(&print_mtx);

  	}

	return 0;
}

int get_clients(struct sockaddr *serverptr,char* IPstring,uint32_t ip,uint16_t port,client_list** clients,char* mirror)
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
    
    strcpy(buf,"GET_CLIENTS");

    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write in get_clients.\n");
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
			fprintf(stderr,"Error in read in get_clients.\n");
			close(sock_request);
			return -2;
		}
		if (bytes == 0)
			return -1;
		ch = request[i];
		i++;
	}

	pthread_mutex_lock(&str_mtx);
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
   	pthread_mutex_unlock(&str_mtx);

   	int N = atoi(token);

   	for(i = 0 ; i < N ; i++)
   	{
   		uint32_t client_ip;
    	uint16_t client_port;

    	bytes = read(sock_request,&client_ip,sizeof(client_ip));
    	if (bytes < 0)
    	{
			fprintf(stderr,"Error in read in get_clients\n");
			close(sock_request);
			return -2;    		
    	}

    	bytes = read(sock_request,&client_port,sizeof(client_port));
    	if (bytes < 0)
    	{
			fprintf(stderr,"Error in read in get_clients\n");
		    close(sock_request);
			return -2;    		
    	}
    	uint32_t mir_ip = client_ip;    	

    	client_ip = ntohl(client_ip);
    	client_port = ntohs(client_port);

    	if (( ntohl(ip) != client_ip) || (port != client_port))
    	{	
    		pthread_mutex_lock(&list_mtx);
    		insert_node(clients,client_port,client_ip);
    		pthread_mutex_unlock(&list_mtx);

    		char mir_client[128];

    		struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET);
    		sprintf(mir_client,"%s/%s_%d",mirror,h->h_name,client_port);
    		if (mkdir(mir_client, 0777) == 0)
				printf("Mirror Directory %s created.\n",mirror );
    	}
   	}
   	pthread_mutex_lock(&list_mtx); 

	pthread_mutex_unlock(&list_mtx);
	close(sock_request );

	return 0;	
}

int log_on(struct sockaddr *serverptr,char* IPstring,uint32_t ip,uint16_t port)
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

    strcpy(buf, "LOG_ON");

    int bytes =0;
    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_on.\n");
    	close(sock_request );
    	return -1 ;
    }

    uint32_t u_ip = ip;

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

	close(sock_request ); 

	pthread_mutex_lock(&print_mtx);
	printf("LOG_ON\n");
	pthread_mutex_unlock(&print_mtx);

	return 0;
}

int log_off(struct sockaddr *serverptr,char* IPstring,uint32_t ip,uint16_t port)
{
	char buf[64];
	int sock_request;

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

    strcpy(buf, "LOG_OFF");

    int bytes =0;
    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_off.\n");
    	close(sock_request );
    	return -1 ;
    }

    uint32_t u_ip = ip;

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
    
    pthread_mutex_lock(&print_mtx);
    printf("LOG_OFF\n");
    pthread_mutex_unlock(&print_mtx);
	
	close(sock_request ); 
	return 0;
}

int user_on(int socket,client_list** clients,char* mirror)
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
   	uint32_t mir_ip = ip;

   	ip = ntohl(ip);
    port = ntohs(port);

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
		place(&shared_buffer , item) ;
		pthread_cond_signal(&cond_nonempty);

		char mir_client[128];

    	struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET);
    	sprintf(mir_client,"%s/%s_%d",mirror,h->h_name,port);
   		if (mkdir(mir_client, 0777)!= 0)
			printf("Error\n");
		else
			printf("Mirror Directory %s created.\n",mirror );

    }
    pthread_mutex_unlock(&list_mtx);

    struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET);

	pthread_mutex_lock(&print_mtx);
    printf("USER_ON %s %d\n",h->h_name,port);
    pthread_mutex_unlock(&print_mtx);

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
   	uint32_t mir_ip = ip;
   	ip = ntohl(ip);
    port = ntohs(port);

   	pthread_mutex_lock(&list_mtx);

    if (search_list(*clients,port,ip) != NULL)
    {	
    	delete_node(clients,port,ip);
    }
    else
    {	pthread_mutex_lock(&print_mtx);
    	printf("ERROR_IP_PORT_NOT_FOUND_IN_LIST\n");
		pthread_mutex_unlock(&print_mtx);
	}

	pthread_mutex_unlock(&list_mtx);

    struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET);

	pthread_mutex_lock(&print_mtx);
    printf("USER_OFF %s %d\n",h->h_name,port);
    pthread_mutex_unlock(&print_mtx);

    return 0;
}