#include "userprog/validate.h"

#include "threads/thread.h"
#include "threads/vaddr.h"

static int64_t get_user(const uint8_t* uaddr);
static int64_t put_user(uint8_t* udst, uint8_t byte);

bool valid_address(const void* uaddr, bool write) {
    if (uaddr == NULL || !is_user_vaddr(uaddr)) return false;
    return (write ? put_user(uaddr, 0) : get_user(uaddr)) != -1;
}

bool check_buffer(void *buffer, unsigned size, bool write){
    char *ptr = (char *)buffer;
    for(unsigned i = 0; i < size; i += PGSIZE){
        if(!valid_address(ptr + i, write)) return false;
    }
    if(!valid_address(ptr + size - 1, write)) return false;
    return true;
}


static int64_t get_user(const uint8_t* uaddr) {
    int64_t result;
    __asm __volatile(
        "movabsq $done_get, %0\n"
        "movzbq %1, %0\n"
        "done_get:\n"
        : "=&a"(result)
        : "m"(*uaddr));
    return result;
}

static int64_t put_user(uint8_t* udst, uint8_t byte) {
    int64_t error_code;
    __asm __volatile(
        "movabsq $done_put, %0\n"
        "movb %b2, %1\n"
        "done_put:\n"
        : "=&a"(error_code), "=m"(*udst)
        : "q"(byte));
    return error_code;
}