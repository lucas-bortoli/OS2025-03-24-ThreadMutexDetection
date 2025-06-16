#include "bortoli_alloc.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

    for (PageIndex i = 0; i < PAGE_COUNT; i++)
    {
        table[i]->object = INVALID_OBJECT_HANDLE;
    }

    printf("Alocador iniciado! %d páginas, heap %zu bytes total (%zu kB) -- table %zu bytes total\n", PAGE_COUNT,
           sizeof(Heap), sizeof(Heap) / 1024, sizeof(PageTable));
}

unsigned int count_free_pages()
{
    unsigned int count = 0;

    for (PageIndex i = 0; i < PAGE_COUNT; i++)
    {
        if (table[i]->object == INVALID_OBJECT_HANDLE)
            count++;
    }

    return count;
}

std::optional<PageIndex> find_first_page(ObjectHandle object, PageIndex start_loc = 0)
{
    for (PageIndex i = start_loc; i < PAGE_COUNT; i++)
    {
        if (table[i]->object == object)
            return i;
    }

    return std::nullopt; // não achamos nenhuma página correspondente
}

ObjectHandle bortoli_alloc(ssize_t count)
{
    ObjectHandle id = makeObjectHandle();

    // divisão ceil: https://stackoverflow.com/a/2745086
    unsigned int needed_page_count = (count + PAGE_SIZE - 1) / PAGE_SIZE;

    printf("bortoli_alloc: count=%zu, handle=%zu, pages=%d\n", count, id, needed_page_count);

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
        // procurar primeira página livre (não alocada)
        auto page_index = find_first_page(INVALID_OBJECT_HANDLE, 0).value();

        table[page_index]->object = id;
        table[page_index]->next_entry = 0; // <-- atualizado no próximo passo
        table[page_index]->used = (unsigned char)std::min(remaining_bytes, (ssize_t)sizeof(Page));

        printf("bortoli_alloc: página %u alocada: %u bytes\n", page_index, table[page_index]->used);

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

    allocator_mutex->lock();

    for (PageIndex i = 0; i < PAGE_COUNT; i++)
    {
        if ((*table)[i].object == handle)
        {
            // para desalocar, é simples: basta tirar a referência ao objeto
            (*table)[i].object = INVALID_OBJECT_HANDLE;
            // a memória ainda está lá, e será sobrescrita quando possível
            printf("bortoli_dealloc: desalocando página %u (objeto %zu)\n", i, handle);
        }
    }

    allocator_mutex->unlock();
}

void bortoli_read(ObjectHandle source, char* target, ssize_t total_bytes)
{
    printf("bortoli_read: source=%zu, target=%p, count=%zu\n", source, target, total_bytes);

    allocator_mutex->lock();

    auto first_page = find_first_page(source);
    if (!first_page.has_value())
    {
        printf("bortoli_read: objeto %zu não está alocado!\n", source);
        exit(1);
    }

    PageIndex source_page = first_page.value();
    ssize_t read_bytes = 0;
    while (read_bytes < total_bytes)
    {
        ssize_t remaining = total_bytes - read_bytes;
        ssize_t copy_amount = std::min(remaining, (ssize_t)sizeof(Page));

        char* source_ptr = (char*)pages[source_page];
        char* dest_ptr = (char*)target + read_bytes;

        printf("bortoli_read: lendo %zu bytes da página %u (%p) para %p\n", copy_amount, source_page, source_ptr,
               dest_ptr);
        std::memcpy(dest_ptr, source_ptr, copy_amount);

        read_bytes += copy_amount;
        source_page = (*table)[source_page].next_entry;
    }

    allocator_mutex->unlock();
}

void bortoli_write(ObjectHandle target, const char* source, ssize_t total_bytes)
{
    printf("bortoli_write: source=%p, target=%zu, count=%zu\n", source, target, total_bytes);

    allocator_mutex->lock();

    auto first_page = find_first_page(target);
    if (!first_page.has_value())
    {
        printf("bortoli_write: objeto %zu não está alocado!\n", target);
        exit(1);
    }

    PageIndex target_page = first_page.value();
    ssize_t copied_bytes = 0;
    while (copied_bytes < total_bytes)
    {
        ssize_t copy_amount = std::min((ssize_t)sizeof(Page), total_bytes - copied_bytes);

        char* source_ptr = (char*)source + copied_bytes;
        char* dest_ptr = (char*)pages[target_page];

        printf("bortoli_write: copiando %zu bytes de %p para página %u (%p)\n", copy_amount, source_ptr, target_page,
               dest_ptr);
        std::memcpy(dest_ptr, source_ptr, copy_amount);

        copied_bytes += copy_amount;
        target_page = (*table)[target_page].next_entry;
    }

    allocator_mutex->unlock();
}

void bortoli_defrag()
{
    printf("bortoli_defrag\n");

    allocator_mutex->lock();

    allocator_mutex->unlock();
}

void bortoli_print_table()
{
    allocator_mutex->lock();

    for (PageIndex i = 0; i < PAGE_COUNT; i++)
    {
        if (table[i]->object == INVALID_OBJECT_HANDLE)
            continue;

        printf("Table[%d]\tObj: %d\n\n", i, table[i]->object);
    }

    allocator_mutex->unlock();
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