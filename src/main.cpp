#include "bortoli_alloc.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <thread>

int main(int argc, char** argv)
{
    bortoli_allocator_init();

    // testes
    // bortoli_write(&handle, handle, 32);

    char* original = "Meu buffer original";
    const uint8_t original_size = strlen(original) + 1; // strlen n considera NUL
    char copia[original_size];

    ObjectHandle handle = bortoli_alloc(16);

    printf("main: minha string original: %s\n", original);

    bortoli_write(handle, original, original_size);
    bortoli_read(handle, copia, original_size);

    printf("main: minha string copia: %s\n", &copia);

    bortoli_dealloc(handle);

    return 0;
}
