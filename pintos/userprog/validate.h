#include <stdbool.h>
#include <stddef.h>

bool valid_address(const void* uaddr, bool write);
bool check_buffer(void *buffer, unsigned size, bool write); 
