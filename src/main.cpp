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

    char* original = "abc";
    const uint8_t original_size = strlen(original) + 1;
    char copia[original_size];

    ObjectHandle handle = bortoli_alloc(16);

    printf("main: minha string original: %s\n", original);

    bortoli_write(handle, original, original_size);
    bortoli_read(handle, copia, original_size);

    printf("main: minha string copia: %s\n", &copia);
    printf("main: minha string copia: %X %X %X %X\n", copia[0], copia[1], copia[2], copia[3]);
    printf("main: minha string original: %s\n", original);

    return 0;
}
