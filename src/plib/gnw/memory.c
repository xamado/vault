#include "plib/gnw/memory.h"

#define _GNU_SOURCE
#include <sys/mman.h>
#ifndef MAP_32BIT
#define MAP_32BIT 0x40
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plib/gnw/debug.h"
#include "plib/gnw/gnw.h"

// A special value that denotes a beginning of a memory block data.
#define MEMORY_BLOCK_HEADER_GUARD 0xFEEDFACE

// A special value that denotes an ending of a memory block data.
#define MEMORY_BLOCK_FOOTER_GUARD 0xBEEFCAFE

// A header of a memory block.
typedef struct MemoryBlockHeader {
    // Size of the memory block including header and footer.
    size_t size;

    // See [MEMORY_BLOCK_HEADER_GUARD].
    int guard;
} MemoryBlockHeader;

// A footer of a memory block.
typedef struct MemoryBlockFooter {
    // See [MEMORY_BLOCK_FOOTER_GUARD].
    int guard;
} MemoryBlockFooter;

static void* my_malloc(size_t size);
static void* my_realloc(void* ptr, size_t size);
static void my_free(void* ptr);
static void* mem_prep_block(void* block, size_t size);
static void mem_check_block(void* block);

typedef struct MemBlock {
    size_t size;
    int is_free;
    struct MemBlock* next;
    struct MemBlock* prev;
} MemBlock;

static MemBlock* heap_head = NULL;
static void* heap_base = NULL;
#define HEAP_SIZE (256 * 1024 * 1024)

static void init_heap() {
    heap_base = mmap(NULL, HEAP_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0);
    if (heap_base == MAP_FAILED) {
        debug_printf("Failed to mmap 256MB for 32-bit heap\n");
        exit(1);
    }
    heap_head = (MemBlock*)heap_base;
    heap_head->size = HEAP_SIZE - sizeof(MemBlock);
    heap_head->is_free = 1;
    heap_head->next = NULL;
    heap_head->prev = NULL;
}

static void* my_malloc_impl(size_t size) {
    if (!heap_head) init_heap();
    size = (size + 15) & ~15;
    MemBlock* curr = heap_head;
    while(curr) {
        if (curr->is_free && curr->size >= size) {
            if (curr->size > size + sizeof(MemBlock) + 32) {
                MemBlock* new_block = (MemBlock*)((unsigned char*)curr + sizeof(MemBlock) + size);
                new_block->size = curr->size - size - sizeof(MemBlock);
                new_block->is_free = 1;
                new_block->next = curr->next;
                new_block->prev = curr;
                if (new_block->next) new_block->next->prev = new_block;
                curr->next = new_block;
                curr->size = size;
            }
            curr->is_free = 0;
            return (unsigned char*)curr + sizeof(MemBlock);
        }
        curr = curr->next;
    }
    return NULL;
}

static void my_free_impl(void* ptr) {
    if (!ptr) return;
    MemBlock* block = (MemBlock*)((unsigned char*)ptr - sizeof(MemBlock));
    block->is_free = 1;
    if (block->next && block->next->is_free) {
        block->size += sizeof(MemBlock) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }
    if (block->prev && block->prev->is_free) {
        block->prev->size += sizeof(MemBlock) + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
    }
}

// 0x51DED0
static MallocProc* p_malloc = my_malloc;

// 0x51DED4
static ReallocProc* p_realloc = my_realloc;

// 0x51DED8
static FreeProc* p_free = my_free;

// 0x51DEDC
static int num_blocks = 0;

// 0x51DEE0
static int max_blocks = 0;

// 0x51DEE4
static size_t mem_allocated = 0;

// 0x51DEE8
static size_t max_allocated = 0;

// 0x4C5A80
char* mem_strdup(const char* string)
{
    char* copy = NULL;
    if (string != NULL) {
        copy = (char*)p_malloc(strlen(string) + 1);
        strcpy(copy, string);
    }
    return copy;
}

// 0x4C5AD0
void* mem_malloc(size_t size)
{
    return p_malloc(size);
}

// 0x4C5AD8
static void* my_malloc(size_t size)
{
    void* ptr = NULL;

    if (size != 0) {
        size += sizeof(MemoryBlockHeader) + sizeof(MemoryBlockFooter);

        unsigned char* block = (unsigned char*)my_malloc_impl(size);
        if (block != NULL) {
            // NOTE: Uninline.
            ptr = mem_prep_block(block, size);

            num_blocks++;
            if (num_blocks > max_blocks) {
                max_blocks = num_blocks;
            }

            mem_allocated += size;
            if (mem_allocated > max_allocated) {
                max_allocated = mem_allocated;
            }
        }
    }

    return ptr;
}

// 0x4C5B50
void* mem_realloc(void* ptr, size_t size)
{
    return p_realloc(ptr, size);
}

// 0x4C5B58
static void* my_realloc(void* ptr, size_t size)
{
    if (ptr != NULL) {
        unsigned char* block = (unsigned char*)ptr - sizeof(MemoryBlockHeader);

        MemoryBlockHeader* header = (MemoryBlockHeader*)block;
        size_t oldSize = header->size;

        mem_allocated -= oldSize;

        mem_check_block(block);

        if (size == 0) {
            my_free_impl(block);
            num_blocks--;
            return NULL;
        }

        size += sizeof(MemoryBlockHeader) + sizeof(MemoryBlockFooter);

        unsigned char* newBlock = (unsigned char*)my_malloc_impl(size);
        if (newBlock != NULL) {
            memcpy(newBlock, block, oldSize < size ? oldSize : size);
            my_free_impl(block);

            mem_allocated += size;
            if (mem_allocated > max_allocated) {
                max_allocated = mem_allocated;
            }

            // NOTE: Uninline.
            ptr = mem_prep_block(newBlock, size);
        } else {
            mem_allocated += oldSize;

            debug_printf("%s,%u: ", __FILE__, __LINE__); // "Memory.c", 195
            debug_printf("Realloc failure.\n");
            ptr = NULL;
        }
    } else {
        ptr = p_malloc(size);
    }

    return ptr;
}

// 0x4C5C24
void mem_free(void* ptr)
{
    p_free(ptr);
}

// 0x4C5C2C
static void my_free(void* ptr)
{
    if (ptr != NULL) {
        void* block = (unsigned char*)ptr - sizeof(MemoryBlockHeader);
        MemoryBlockHeader* header = (MemoryBlockHeader*)block;

        mem_check_block(block);

        mem_allocated -= header->size;
        num_blocks--;

        my_free_impl(block);
    }
}

// NOTE: Not used.
//
// 0x4C5C5C
void mem_check()
{
    if (p_malloc == my_malloc) {
        debug_printf("Current memory allocated: %6d blocks, %9u bytes total\n", num_blocks, mem_allocated);
        debug_printf("Max memory allocated:     %6d blocks, %9u bytes total\n", max_blocks, max_allocated);
    }
}

// NOTE: Unused.
//
// 0x4C5CA8
void mem_register_func(MallocProc* mallocFunc, ReallocProc* reallocFunc, FreeProc* freeFunc)
{
    if (!GNW_win_init_flag) {
        p_malloc = mallocFunc;
        p_realloc = reallocFunc;
        p_free = freeFunc;
    }
}

// NOTE: Inlined.
//
// 0x4C5CC4
static void* mem_prep_block(void* block, size_t size)
{
    MemoryBlockHeader* header;
    MemoryBlockFooter* footer;

    header = (MemoryBlockHeader*)block;
    header->guard = MEMORY_BLOCK_HEADER_GUARD;
    header->size = size;

    footer = (MemoryBlockFooter*)((unsigned char*)block + size - sizeof(*footer));
    footer->guard = MEMORY_BLOCK_FOOTER_GUARD;

    return (unsigned char*)block + sizeof(*header);
}

// Validates integrity of the memory block.
//
// [block] is a pointer to the the memory block itself, not it's data.
//
// 0x4C5CE4
static void mem_check_block(void* block)
{
    MemoryBlockHeader* header = (MemoryBlockHeader*)block;
    if (header->guard != MEMORY_BLOCK_HEADER_GUARD) {
        debug_printf("Memory header stomped.\n");
    }

    MemoryBlockFooter* footer = (MemoryBlockFooter*)((unsigned char*)block + header->size - sizeof(MemoryBlockFooter));
    if (footer->guard != MEMORY_BLOCK_FOOTER_GUARD) {
        debug_printf("Memory footer stomped.\n");
    }
}
