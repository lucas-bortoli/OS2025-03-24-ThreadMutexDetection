#include "bortoli_alloc.h"
#include <cstdint>
#include <cstdio>

int main(int argc, char** argv)
{
    bortoli_allocator_init();

    // testes
    // bortoli_write(&handle, handle, 32);

    ObjectHandle handle = bortoli_alloc(130);
    bortoli_dealloc(handle);

    handle = bortoli_alloc(32);
    bortoli_dealloc(handle);

    return 0;
}
