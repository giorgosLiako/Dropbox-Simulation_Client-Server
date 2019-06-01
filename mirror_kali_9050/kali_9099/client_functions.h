
#include "list_interface.h"
#include "buffer_interface.h"

int log_on(struct sockaddr *,char *,uint32_t ,uint16_t);
int log_off(struct sockaddr *,char* ,uint32_t ,uint16_t );
int user_on(int ,client_list**,char*);
int user_off(int ,client_list**);
int get_clients(struct sockaddr *,char*,uint32_t ,uint16_t,client_list**,char*);
int send_get_file_list(buffer_item* ,char*);
int responde_get_file_list(int ,char* );
int send_get_file(buffer_item* , client_list* ,char* );
int responde_get_file(int ,client_list **,char*);