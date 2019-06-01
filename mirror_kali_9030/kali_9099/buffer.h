
#ifndef __BUFFER__
#define __BUFFER__

#include <stdint.h>
#include <time.h>

typedef struct item {

	char pathname[128];
	time_t version;
	uint32_t ip;
	uint16_t port;

}buffer_item;


typedef struct {

	buffer_item** items;
	int start;
	int end;
	int count;
	int buffer_size;

} buffer;

#endif