
#ifndef __BUFFER__
#define __BUFFER__

#include <stdint.h>
#include <time.h>

/*the buffer item*/
typedef struct item {

	char pathname[128];
	time_t version;
	uint32_t ip;
	uint16_t port;

}buffer_item;

/*the buffer and the information we need*/
typedef struct {

	buffer_item** items;
	int start;
	int end;
	int count;
	int buffer_size;

} buffer;

#endif