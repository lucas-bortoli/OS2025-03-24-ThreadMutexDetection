#include "bortoli_alloc.h"
#include <cstdint>
#include <cstdio>

int main(int argc, char** argv)
{
    bortoli_allocator_init();

    // testes
    // ObjectHandle handle = bortoli_alloc(32);
    // bortoli_write(&handle, handle, 32);

    bortoli_alloc(PAGE_SIZE * PAGE_COUNT);

    bortoli_alloc(1);

    // bortoli_dealloc(handle);

    return 0;
}
