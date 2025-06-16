#ifndef _H_BALLOC
#define _H_BALLOC

#include <cstdint>
#include <cstdio>
#include <optional>

#define PAGE_SIZE 8
#define PAGE_COUNT 32

typedef unsigned long ObjectHandle;

extern const ObjectHandle INVALID_OBJECT_HANDLE;

struct PageTableEntry
{
    ObjectHandle object;
    unsigned int next_entry; /* semântica: aponta pra ele mesmo se não há uma "próxima" página do objeto. */
    unsigned char used;
};

typedef char Page[PAGE_SIZE];

void bortoli_allocator_init();

ObjectHandle bortoli_alloc(ssize_t count);
void bortoli_dealloc(ObjectHandle handle);
void bortoli_read(ObjectHandle source, char* target, ssize_t bytes);
void bortoli_write(ObjectHandle target, const char* source, ssize_t total_bytes);
void bortoli_defrag();

void bortoli_print_table();

#endif