#include "bortoli_alloc.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stack>

ObjectHandle makeObjectHandle()
{
    static std::atomic<ObjectHandle> counter(1); // começar counter em 1; zero indica uma handle inválida

    return counter.fetch_add(1, std::memory_order_relaxed);
}

const ObjectHandle INVALID_OBJECT_HANDLE = 0;

typedef unsigned int PageIndex;
typedef PageTableEntry PageTable[PAGE_COUNT];
typedef Page Heap[PAGE_COUNT];

std::mutex* allocator_mutex;

PageTable* table;
Heap* pages;

void bortoli_allocator_init()
{
    allocator_mutex = new std::mutex();

    table = (PageTable*)std::malloc(sizeof(PageTable));
    pages = (Heap*)std::malloc(sizeof(Heap));

    printf("Alocador iniciado! %d páginas, heap %zu bytes total (%zu kB) -- table %zu bytes total\n", PAGE_COUNT,
           sizeof(Heap), sizeof(Heap) / 1024, sizeof(PageTable));
}

unsigned int count_free_pages()
{
    unsigned int count = 0;

    for (PageIndex i = 0; i < PAGE_COUNT; i++)
    {
        if ((*table)[i].object == 0)
            count++;
    }

    return count;
}

std::optional<PageIndex> find_free_page_index(PageIndex start_loc = 0)
{
    for (PageIndex i = start_loc; i < PAGE_COUNT; i++)
    {
        if ((*table)[i].object == INVALID_OBJECT_HANDLE)
            return i;
    }

    return std::nullopt; // não achamos nenhuma página livre
}

ObjectHandle bortoli_alloc(ssize_t count)
{
    ObjectHandle id = makeObjectHandle();

    // divisão ceil: https://stackoverflow.com/a/2745086
    unsigned int needed_page_count = (count + PAGE_SIZE - 1) / PAGE_SIZE;

    printf("bortoli_alloc: count=%zu, object=%zu, pages=%d\n", count, id, needed_page_count);

    // vamos alocar N páginas na tabela
    allocator_mutex->lock();

    if (count_free_pages() < needed_page_count)
    {
        printf("bortoli_alloc: out of memory: não há páginas suficientes! páginas livres: %d, páginas necessárias: %d "
               "(requisitado: %zu "
               "bytes)\n",
               count_free_pages(), needed_page_count, count);
        allocator_mutex->unlock();
        return INVALID_OBJECT_HANDLE;
    }

    std::stack<PageIndex> allocated;
    ssize_t remaining_bytes = count;
    while (remaining_bytes > 0)
    {
        // TODO não escanear do zero para TODAS as páginas, armazenar último index
        auto page_index = find_free_page_index(0).value();

        PageTableEntry entry = {
            .object = id,
            .next_entry = 0, // <-- atualizado no próximo passo
            .used = (unsigned char)std::min(remaining_bytes, (ssize_t)sizeof(Page)),
        };

        (*table)[page_index] = entry;

        printf("bortoli_alloc: página %u alocada: %u bytes\n", page_index, entry.used);

        allocated.push(page_index);
        remaining_bytes -= sizeof(Page);
    }

    // corrigir next_entry para todas as páginas alocadas
    if (allocated.size() > 0)
    {
        PageIndex last = allocated.top();
        while (allocated.size() > 0)
        {
            PageIndex this_el = allocated.top();
            (*table)[this_el].next_entry = last;
            printf("bortoli_alloc: %u -> %u\n", this_el, last);
            last = this_el;
            allocated.pop();
        }
    }

    allocator_mutex->unlock();

    return id;
}

void bortoli_dealloc(ObjectHandle handle)
{
    printf("bortoli_dealloc: handle=%zu\n", handle);
}

void bortoli_read(ObjectHandle source, void* target, ssize_t bytes)
{
    printf("bortoli_read: source=%zu, target=%p, count=%zu\n", source, target, bytes);
}

void bortoli_write(void* source, ObjectHandle target, ssize_t bytes)
{
    printf("bortoli_write: source=%p, target=%zu, count=%zu\n", source, target, bytes);
}

void bortoli_defrag()
{
    printf("bortoli_defrag\n");
}

void defrag()
{
    /**
     * primeiro analisamos o uso de páginas na heap para computar a organização "ideal" das páginas (quicksort)
     *
     * depois iteramos sobre a heap fazendo swap entre a página atual e a página "correta" (segundo o computado)
     */

    /**
     * qu
     */
}