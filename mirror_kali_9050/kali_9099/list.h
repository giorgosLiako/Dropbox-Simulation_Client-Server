
/*file list.h*/
#ifndef __LIST__
#define __LIST__

#include <stdint.h>

typedef struct list_node {

	uint32_t ip;
	uint16_t port;
	struct list_node * next_node;

} client_list;

#endif