/*file with the implementation of the requests that sends a receives a client*/

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

/*extern global variables that are declared in dropbox_client.c*/
extern buffer shared_buffer ;
extern pthread_mutex_t list_mtx;
extern pthread_mutex_t str_mtx;
extern pthread_mutex_t print_mtx;
extern pthread_cond_t cond_nonempty;

/*function to send a get_file_list request to another client*/
int send_get_file_list(buffer_item* item ,char* mirror)
{	
	char buf[64];
	int sock ;
	/*make a new socket*/
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
   	{
   		fprintf(stderr,"Error in socket\n");
    	return -1;
   	}

    struct sockaddr_in  other_client;
    struct sockaddr *other_client_ptr = (struct sockaddr*)&other_client;

    /*set the struct with the iformations of the other client*/
	bzero(&other_client, sizeof(other_client));
    other_client.sin_family = AF_INET;       /* Internet domain */
    other_client.sin_port = htons(item->port); /* other_client port */
  	other_client.sin_addr.s_addr = htonl(item->ip);/*other client ip*/

	/*make the connection*/
    if (connect(sock, other_client_ptr, sizeof(struct sockaddr_in)) < 0)
    {
    	fprintf(stderr,"Error in connect\n");
    	close(sock);
    	return -1;
    }

    /*lock with the mutex str every function of string.h*/
	pthread_mutex_lock(&str_mtx);
	strcpy(buf,"GET_FILE_LIST");
	pthread_mutex_unlock(&str_mtx);

	/*send GET_FILE_LIST */
	int bytes = write(sock,buf,strlen(buf)+1);
	if (bytes < 0)
	{
		fprintf(stderr,"Error in write in send_get_file_list.\n");
		close(sock);
		return -1;	
	}

	int i = 0 ;
	char ch = 'z';

	/*read the responded string*/
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
	/*buf is in the form of "FILE_LIST N" , N is the number of files*/
	char *token;

	pthread_mutex_lock(&str_mtx);   
   	token = strtok(buf," ");
   	/*take the number N of files*/
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
   	/*for each file read its details*/
   	for (int i = 0 ; i < N ; i++)
   	{	
   		int j = 0 ;
		ch = 'z';
		/*read the name_size of the file*/
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
		/*take the name_size*/
		int name_size = atoi(buf);

		/*read the path of the file , which is name_size bytes*/
		bytes = read(sock,buf,name_size);
		if (bytes < 0)
		{
			fprintf(stderr, "Error in read in send_get_file_list. \n");
			return -2;
		}

		/*make a new buffer_item*/
		buffer_item * new_item = (buffer_item*) malloc(sizeof(buffer_item));
		if (new_item == NULL)
		{
			fprintf(stderr, "Error in malloc in send_get_file_list.\n" );
			return -3;
		}

		/*get the hostname to generate the path of the file in the mirror directory*/
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
		/*read the version */
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
		/*take the version , version is the last modification time*/
		new_item->version = atoi(buf);

		new_item->port = item->port;   	
		new_item->ip  = item->ip;
		place(&shared_buffer , new_item) ; /*place the new item in the buffer*/
		pthread_cond_signal(&cond_nonempty); /*signal a thread that the buffer is not empty*/
	
		counter++;
	}

	/*lock in print to hace a logical output*/
	pthread_mutex_lock(&print_mtx);
	
	if (counter == N)
		printf("The whole file list has been received.\n");
	else
		printf("ERROR: Number of files in file list: %d \tReceived files of file list: %d\n",N,counter );

	pthread_mutex_unlock(&print_mtx);

    close(sock);
	return 0 ;
}

/*function to responde a client to a get_file_list request from another client*/
int responde_get_file_list(int socket,char* dir)
{
	char buf[64];

	/*count the files of the dir directory*/
	int number_of_files = files_counter(dir,NULL);

	sprintf(buf,"FILE_LIST %d",number_of_files);
	/*send "FILE LIST N"*/
	write(socket,buf,strlen(buf)+1);

	/*send the pathname and version of each file*/
	int paths_sent = send_pathnames(dir,NULL,socket);

	/*confirm that all the file list has been sent*/
	pthread_mutex_lock(&print_mtx);
	if (paths_sent == number_of_files)
		printf("The whole file list has been sent\n");
	else
		printf("ERROR: Number of files in file_list: %d \tSent files of file list: %d\n",number_of_files,paths_sent );
	pthread_mutex_unlock(&print_mtx);
	return 0;
}

/*function to send a get_file request to another client */
int send_get_file(buffer_item* item, client_list* clients,char* mirror)
{
	/*check if the client that has the item is in the list*/
	if (search_list(clients,item->port,item->ip) == NULL)
	{
		printf("Client is not in the list.\n");
		return -1;
	} 

	uint32_t mir_ip = htonl(item->ip); /*to get the hostname */
	char path[256];
	struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET);
    
    /*get the hostname to make the path in the mirror directory */
    pthread_mutex_lock(&str_mtx);
    sprintf(path,"%s/%s_%d/%s",mirror,h->h_name,item->port,item->pathname);
    pthread_mutex_unlock(&str_mtx);
	
	int exist = 0;
	struct stat  st;   /*call stat to check if the file exists*/
  	if (stat (path, &st) != 0)
  		exist = 0;
  	else
  		exist = 1;

	char buf[256];
	int sock ;

	/*make a new socket*/
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
   	{
   		fprintf(stderr,"Error in socket\n");
    	return -1;
   	}

    struct sockaddr_in  other_client;
    struct sockaddr *other_client_ptr = (struct sockaddr*)&other_client;

    /*set the struct with the other client's informations */
	bzero(&other_client, sizeof(other_client));
    other_client.sin_family = AF_INET;       /* Internet domain */
    other_client.sin_port = htons(item->port); /* other_client port */
  	other_client.sin_addr.s_addr = htonl(item->ip);//htonl(temp->ip);

  	/*connect to the other client*/
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

	/*send "GET_FILE"*/
	bytes = write(sock,buf,strlen(buf)+1);
	if (bytes < 0)
	{
		fprintf(stderr,"Error in write in send_get_file.\n");
		return -3;
	}
	
	if (exist == 0) /*if file does not exist*/
	{
		pthread_mutex_lock(&str_mtx);
		sprintf(buf,"%s 0",item->pathname);
		pthread_mutex_unlock(&str_mtx);

		/*send "pathname 0" (version is 0 when a file does not exist )*/
		bytes = write(sock,buf,strlen(buf)+1);
		if (bytes < 0)
		{
			fprintf(stderr,"Error in write in send_get_file.\n");
			return -3;
		}
	}
	else /*if file exists */
 	{	
		pthread_mutex_lock(&str_mtx);	/*version is the last modification time*/
		sprintf(buf,"%s %ld",item->pathname,st.st_mtim.tv_sec);
		pthread_mutex_unlock(&str_mtx);

		/*send "pathname version" */
		bytes = write(sock,buf,strlen(buf)+1);
		if (bytes < 0)
		{
			fprintf(stderr,"Error in write in send_get_file.\n");
			return -3;
		}
		
	}
	
	int i = 0 ;
	char ch = 'z';
	/*read the response of the other client*/
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
	{	/*received that the file not found from the other client*/
		pthread_mutex_lock(&print_mtx);
		printf("FILE %s NOT FOUND\n",path);
		pthread_mutex_unlock(&print_mtx);
	}
	else if (!strcmp(buf,"FILE_SIZE"))
	{	/*received "FILE_SIZE" , which means that will follow the version and next the file-size in bytes and the whole file*/
		i = 0 ;
		ch = 'z';
		/*read the "version N"*/
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
   
   		token = strtok(buf," "); /*token the string in space */
   		time_t version = (time_t) atoi(token); /*take the version*/
   		int n ;
   		if (token != NULL)
   		{	token = strtok(NULL," ");
   			n = atoi(token); /*take the file size in bytes */
   		}
   		pthread_mutex_unlock(&str_mtx);

   		/* open the file  or create and open it if it does not exist */
   		int fd = open(path,O_CREAT | O_WRONLY , 0777);
   		if (fd < 0)
   		{	printf("%s\n",path);
   			fprintf(stderr,"Error in open in send_get_file.\n");
   			return -3;
   		}

   		int count = 0;
   		for(int j = 0 ; j < n ; j++)
   		{
   			read(sock,buf,1); /*read each byte and write it to the file*/
   			write(fd,buf,1);
   			count++;
   		}

   		/*confirm that all the bytes transfered */
   		pthread_mutex_lock(&print_mtx);
   		if (count == n)
   			printf("File %s received successfully\n",path );
   		pthread_mutex_unlock(&print_mtx);

   		/*set the last modification time and last access time equal to last version*/
   		struct utimbuf ut;
   		ut.actime = version;
   		ut.modtime = version;
   		utime(path, &ut); /*change the times in the OS settings */
	}
	else if (!strcmp(buf,"FILE_UP_TO_DATE"))
	{	/*received a "FILE_UP_TO_DATE" so the file has not changed */
		pthread_mutex_lock(&print_mtx);
		printf("FILE %s UP TO DATE\n",path );
		pthread_mutex_unlock(&print_mtx);
	}

	return 0;
}

/*function to responde to a "get_file" request from another client */
int responde_get_file(int sock,client_list ** clients,char* dir)
{
	char buf[128];
	char pathname[128];
	int i = 0 ,bytes;
	char ch = 'z';

	/*read the "pathname version" */
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

	token = strtok(buf," "); /*token the string in space */
	time_t version;
	if (token != NULL)
	{	strcpy(pathname,token); /*the pathname */
		token = strtok(NULL," ");
		if (token != NULL)
			version = (time_t) atoi(token); /*the version */
	}

	/*make the full path*/
	strcpy(buf,dir);
	strcat(buf,"/");
	strcat(buf,pathname);

	strcpy(pathname,buf);
	pthread_mutex_unlock(&str_mtx);

	struct stat  st;   
  	if (stat (pathname, &st) != 0) /*check to find the path */
  	{	
  		strcpy(buf,"FILE_NOT_FOUND");
  		/*if not found send "FILE_NOT_FOUND"*/
  		bytes = write(sock,buf,strlen(buf)+1);
  		if (bytes < 0 )
  		{
  			fprintf(stderr,"Error in write in responde_get_file.");
  			return -1;
  		}
  		return 0;
  	}

  	if ( st.st_mtim.tv_sec == version) 	/*if the version is the same*/
  	{									/*send "FILE_UP_TO_DATE"*/
  		strcpy(buf,"FILE_UP_TO_DATE");
  		bytes = write(sock,buf,strlen(buf)+1);
  		if (bytes < 0 )
  		{
  			fprintf(stderr,"Error in write in responde_get_file.");
  			return -1;
  		}
  		return 0;  		
  	}
  	else if ( st.st_mtim.tv_sec > version )/*if the version is older than the last modification time*/
  	{
  		strcpy(buf,"FILE_SIZE");
  		/*send "FILE_SIZE"*/
   		bytes = write(sock,buf,strlen(buf)+1);
  		if (bytes < 0 )
  		{
  			fprintf(stderr,"Error in write in responde_get_file.");
  			return -1;
  		}
  		/*send "version size" */
  		sprintf(buf,"%ld %ld",st.st_mtim.tv_sec,st.st_size);
   		bytes = write(sock,buf,strlen(buf)+1);
  		if (bytes < 0 )
  		{
  			fprintf(stderr,"Error in write in responde_get_file.");
  			return -1;
  		}

  		/*open the file */
   		int fd = open(pathname,O_RDONLY);
   		if (fd < 0)
   		{
   			fprintf(stderr,"Error in open in send_get_file.");
   			return -3;
   		}

   		int count = 0;
   		for(int j = 0 ; j < st.st_size ; j++)
   		{
   			read(fd,buf,1); /*read each byte from the file and send it to the socket*/
   			write(sock,buf,1);
   			count++;
   		}

   		/*confirm that all the bytes transfered */
   		pthread_mutex_lock(&print_mtx);
   		if (count == st.st_size)
   			printf("File %s sent successfully\n",pathname );
   		pthread_mutex_unlock(&print_mtx);

  	}

	return 0;
}

/*function to send a get_clients request to server */
int get_clients(struct sockaddr *serverptr,char* IPstring,uint32_t ip,uint16_t port,client_list** clients,char* mirror)
{
	int bytes;
	int sock_request;
	char buf[64];

	/*make a socket*/
	if ((sock_request = socket(PF_INET, SOCK_STREAM, 0)) < 0)
   	{
   		fprintf(stderr,"Error in socket\n");
    	return -1;
   	}

   	/*connect to server */
    if (connect(sock_request, serverptr, sizeof(struct sockaddr_in)) < 0)
    {
    	fprintf(stderr,"Error in connect\n");
    	close(sock_request);
    	return -1;
    }
    
    strcpy(buf,"GET_CLIENTS");
    /*send "GET_CLIENTS "*/
    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write in get_clients.\n");
    	close(sock_request);
    	return -1;
    }

 	char request[32];
	int i = 0 ;
	char ch = 'z';
	/*read "CLIENT_LIST N " , N is the number of the clients in client list */
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
   
   	token = strtok(request," "); /*token the string in space */

   	if (token != NULL)
   		token = strtok(NULL," "); 
   	else
   	{
   		printf("Something went wrong, invalid message from server.\n");
   		close(sock_request);
   		return -3;
   	}
   	pthread_mutex_unlock(&str_mtx);

   	int N = atoi(token); /*take the number of the clients */

   	for(i = 0 ; i < N ; i++) /*for each client */
   	{
   		uint32_t client_ip;
    	uint16_t client_port;

    	/*read the client's ip*/
    	bytes = read(sock_request,&client_ip,sizeof(client_ip));
    	if (bytes < 0)
    	{
			fprintf(stderr,"Error in read in get_clients\n");
			close(sock_request);
			return -2;    		
    	}
    	/*read the client's port */
    	bytes = read(sock_request,&client_port,sizeof(client_port));
    	if (bytes < 0)
    	{
			fprintf(stderr,"Error in read in get_clients\n");
		    close(sock_request);
			return -2;    		
    	}
    	uint32_t mir_ip = client_ip;    	

    	/*convert ip and port to host formation*/
    	client_ip = ntohl(client_ip);
    	client_port = ntohs(client_port);
   		
    	/*check if the new client's information are not the same with the current client that send the GET_CLIENTS request*/
    	if (( ntohl(ip) != client_ip) || (port != client_port))
    	{	
    		pthread_mutex_lock(&list_mtx);
    		insert_node(clients,client_port,client_ip); /*insert the client in the client_list*/
    		pthread_mutex_unlock(&list_mtx);

    		char mir_client[128];

    		struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET);/*mir_ip is in network byte form*/
    		sprintf(mir_client,"%s/%s_%d",mirror,h->h_name,client_port);
    		/*make the client's mirror directory where will placed his files*/
    		if (mkdir(mir_client, 0777) == 0)
				printf("Mirror Directory %s created.\n",mirror );
    	}
   	}


	close(sock_request ); /*close connection*/

	return 0;	
}

/*function to send a LOG_ON request to server */
int log_on(struct sockaddr *serverptr,char* IPstring,uint32_t ip,uint16_t port)
{
	char buf[64];
	int sock_request;
	/*make a socket*/
	if ((sock_request = socket(PF_INET, SOCK_STREAM, 0)) < 0)
   	{
   		fprintf(stderr,"Error in socket in log_on.\n");
    	return -1 ;
   	}
   	/*connect to server */
    if (connect(sock_request, serverptr, sizeof(struct sockaddr_in)) < 0)
    {
    	fprintf(stderr,"Error in connect in log_on.\n");
    	close(sock_request); 
    	return -1 ;
    }	

    strcpy(buf, "LOG_ON");

    int bytes =0;
    /*send "LOG_ON" to server*/
    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_on.\n");
    	close(sock_request );
    	return -1 ;
    }

    uint32_t u_ip = ip;
    /*send the client's ip to server */
    /*ip is already in network byte form from inet_addr function*/
    if ( (bytes = write(sock_request , &u_ip, sizeof(u_ip)) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_on.\n");
    	close(sock_request );
    	return -1 ;
    }

    uint16_t u_port =  htons(port); /*convert port to network byte form */

    /*send the client's port to server*/
    if ( ( bytes = write(sock_request , &u_port, sizeof(u_port)) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_on.\n");
    	close(sock_request );
    	return -1 ;
    }

	close(sock_request ); /*close connection*/

	pthread_mutex_lock(&print_mtx);
	printf("LOG_ON\n");
	pthread_mutex_unlock(&print_mtx);

	return 0;
}

/*function to send LOG_OFF request to server*/
int log_off(struct sockaddr *serverptr,char* IPstring,uint32_t ip,uint16_t port)
{
	char buf[64];
	int sock_request;

	/*make a socket*/
	if ((sock_request = socket(PF_INET, SOCK_STREAM, 0)) < 0)
   	{
   		fprintf(stderr,"Error in socket in log_off.\n");
    	return -1 ;
   	}

   	/*connect to server */
    if (connect(sock_request, serverptr, sizeof(struct sockaddr_in)) < 0)
    {
    	fprintf(stderr,"Error in connect in log_off.\n");
    	close(sock_request); 
    	return -1 ;
    }	

    strcpy(buf, "LOG_OFF");

    int bytes =0;
    /*send "LOG_OFF" to server*/
    if ( (bytes = write(sock_request ,buf,strlen(buf)+1) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_off.\n");
    	close(sock_request );
    	return -1 ;
    }

    uint32_t u_ip = ip;
    /*send the client's ip to server */
    /*ip is already in network byte form from inet_addr function*/
    if ( (bytes = write(sock_request , &u_ip, sizeof(u_ip)) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_off.\n");
    	close(sock_request );
    	return -1 ;
    }

    uint16_t u_port =  htons(port); /*convert port to network byte form */

    /*send the client's port to server*/
    if ( ( bytes = write(sock_request , &u_port, sizeof(u_port)) ) < 0 )
    {
    	fprintf(stderr, "Error in write in log_off.\n");
    	close(sock_request );
    	return -1 ;
    }
    
    pthread_mutex_lock(&print_mtx);
    printf("LOG_OFF\n");
    pthread_mutex_unlock(&print_mtx);
	
	close(sock_request ); /*close connection*/
	return 0;
}

/*function to service the USER_ON request that asked from server*/
int user_on(int socket,client_list** clients,char* mirror)
{

	uint32_t ip;
    uint16_t port;

    int read_bytes;
    /*read the ip of the client that connected in the application*/
    read_bytes = read(socket,&ip,sizeof(ip));
    if (read_bytes < 0)
    {
		fprintf(stderr,"Error in read in user_on\n");
		return -2;    		
   	}
    /*read the port of the client that connected in the application*/
    read_bytes = read(socket,&port,sizeof(port));
    if (read_bytes < 0)
    {
		fprintf(stderr,"Error in read in user on\n");
		return -2;    		
   	}
   	uint32_t mir_ip = ip;

   	ip = ntohl(ip); /*convert ip and port to host form */
    port = ntohs(port);

    pthread_mutex_lock(&list_mtx);
    /*search the list to not have already this new client*/
    if (search_list(*clients,port,ip) == NULL)
    {	
    	insert_node(clients,port,ip); /*insert the new client in the clients list*/
 		buffer_item * item = (buffer_item*) malloc(sizeof(buffer_item));
		if (item == NULL)
		{
			fprintf(stderr, "Error in malloc in user_on in client_functions.c\n");
			return -2;
		}
		/*make a new buffer_item */
		item->ip = ip;	
		item->port = port;
		item->pathname[0] = '\0';
		item->version = 0;
		place(&shared_buffer , item) ; /*place the new item in the buffer*/
		pthread_cond_signal(&cond_nonempty); /*signal one thread that the buffer is not empty*/

		char mir_client[128];

    	struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET);/*mir_ip is in network byte form*/
    	sprintf(mir_client,"%s/%s_%d",mirror,h->h_name,port);
    	/*make the mirror directory of the new client where will place his files*/
   		if (mkdir(mir_client, 0777)!= 0)
			printf("Error\n");
		else
			printf("Mirror Directory %s created.\n",mirror );

    }
    pthread_mutex_unlock(&list_mtx);

    struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET);/*mir_ip is in network byte form*/

	pthread_mutex_lock(&print_mtx);
    printf("USER_ON %s %d\n",h->h_name,port);
    pthread_mutex_unlock(&print_mtx);

    return 0;
}

/*function to service the USER_OFF request that asked from server*/
int user_off(int socket,client_list** clients)
{
	uint32_t ip;
    uint16_t port;

    int read_bytes;
    /*read the exited client's ip*/
    read_bytes = read(socket,&ip,sizeof(ip));
    if (read_bytes < 0)
    {
		fprintf(stderr,"Error in read in user_on\n");
		return -2;    		
   	}
   	/*read the exited client's port*/
    read_bytes = read(socket,&port,sizeof(port));
    if (read_bytes < 0)
    {
		fprintf(stderr,"Error in read in user on\n");
		return -2;    		
   	}
   	uint32_t mir_ip = ip;
   	ip = ntohl(ip); /*convert ip and port to host form */
    port = ntohs(port);

   	pthread_mutex_lock(&list_mtx);

    if (search_list(*clients,port,ip) != NULL)/*search the list and if the client exists in the list*/
    {									/*delete him */
    	delete_node(clients,port,ip);
    }
    else /*if he does not exist in the list print an error */
    {	pthread_mutex_lock(&print_mtx);
    	printf("ERROR_IP_PORT_NOT_FOUND_IN_LIST\n");
		pthread_mutex_unlock(&print_mtx);
	}

	pthread_mutex_unlock(&list_mtx);

    struct hostent* h = gethostbyaddr(&mir_ip,sizeof(mir_ip),AF_INET); /*mir_ip is in network byte form*/

	pthread_mutex_lock(&print_mtx);
    printf("USER_OFF %s %d\n",h->h_name,port);
    pthread_mutex_unlock(&print_mtx);

    return 0;
}