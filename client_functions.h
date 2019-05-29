
#include "list_interface.h"
#include "buffer_interface.h"

int log_on(struct sockaddr *,char *,uint32_t ,uint16_t);
int log_off(struct sockaddr *,char* ,uint32_t ,uint16_t );
int user_on(int ,client_list**,buffer*);
int user_off(int ,client_list**);
int get_clients(struct sockaddr *,char*,uint32_t ,uint16_t,client_list**);
int send_get_file_list(buffer_item* );
int responde_get_file_list(int ,char* );
int get_file(int , client_list** );