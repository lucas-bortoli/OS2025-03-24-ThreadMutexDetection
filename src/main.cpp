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

    while (true)
    {
        ObjectHandle handle = bortoli_alloc(16);

        printf("main: handle %zu\n", handle);

        const char* texto = "Hello Goiás!";

        printf("main: minha string: %s\n", texto);

        bortoli_write(handle, &texto, strlen(texto));

        // talvez desalocar, talvez não...
        if (std::rand() % 4 > 2)
            bortoli_dealloc(handle);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
