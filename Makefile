
all : dropbox_server dropbox_client

list.o : list.c list.h
	gcc -c list.c

dropbox_server.o : dropbox_server.c
	gcc -c dropbox_server.c

dropbox_client.o : dropbox_client.c
	gcc -c dropbox_client.c

dropbox_server : dropbox_server.o list.o
	gcc -o dropbox_server dropbox_server.o list.o -g3 -Wall


dropbox_client : dropbox_client.o list.o
	gcc -o dropbox_client dropbox_client.o list.o -g3 -Wall

clean :
	rm dropbox_client dropbox_server dropbox_server.o dropbox_client.o list.o