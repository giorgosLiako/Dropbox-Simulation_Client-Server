
#include "list.h"

struct list_node* search_list(client_list * , uint16_t , uint32_t );
int insert_node(client_list ** , uint16_t , uint32_t );
void delete_node(client_list **  , uint16_t , uint32_t );
void destroy_list(client_list**);
int counter_nodes(client_list*,uint16_t,uint32_t);