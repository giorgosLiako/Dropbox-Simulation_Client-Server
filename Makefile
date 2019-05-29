
all : dropbox_server dropbox_client

list.o : list.c list.h
	gcc -c list.c -g -Wall

dropbox_server.o : dropbox_server.c
	gcc -c dropbox_server.c -g -Wall

server_functions.o : server_functions.c
	gcc -c server_functions.c -g -Wall

client_functions.o : client_functions.c client_functions.h
	gcc -c client_functions.c -g -Wall -pthread

dropbox_client.o : dropbox_client.c
	gcc -c dropbox_client.c -g -Wall -pthread

file_functions.o : file_functions.c
	gcc -c file_functions.c -g -Wall

buffer.o : buffer.c buffer.h
	gcc -c buffer.c -g -Wall

dropbox_server : dropbox_server.o list.o server_functions.o
	gcc -o dropbox_server dropbox_server.o list.o server_functions.o -g -Wall

dropbox_client : dropbox_client.o list.o client_functions.o buffer.o file_functions.o
	gcc -o dropbox_client dropbox_client.o list.o client_functions.o buffer.o file_functions.o -g -Wall -pthread

clean :
	rm dropbox_client dropbox_server dropbox_server.o dropbox_client.o list.o server_functions.o client_functions.o buffer.o file_functions.o