
#include "buffer.h"

void initialize_buffer(buffer* ,int);
void destroy_buffer(buffer* );
buffer_item* obtain(buffer * );
void place(buffer * , buffer_item * );