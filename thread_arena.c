#include "thread_arena.h"
#include "global_pool_manager.h"
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>

int thread_find_arena_size(void* block) {
    block_header* header = (block_header*)((char*)block - HEADER_SIZE);
    return header->size;
}

void* thread_arena_alloc(thread_arena* arena, int size) {
    //round-off size for better memory alignment
    int aligned_size = (size + 7) & ~7;
    if (arena->cur_empty_space < aligned_size) {
        void* new_block = global_pool_manager_arena_alloc(aligned_size + THREAD_BLOCK_HEADER_SIZE);
        if (new_block == NULL) {
            printf("Failed to allocate memory\n");
            return NULL;
        }
        thread_block_header* new_block_header = (thread_block_header*)new_block;
        new_block_header->next_block = arena->cur_block;
        arena->cur_block = new_block;
        arena->offset = (char*)new_block + THREAD_BLOCK_HEADER_SIZE;
        arena->cur_empty_space = thread_find_arena_size(new_block) - THREAD_BLOCK_HEADER_SIZE;
    }
    void* ret = arena->offset;
    arena->offset = (char*)arena->offset + aligned_size;
    arena->cur_empty_space -= aligned_size;
    return ret;
}

thread_arena* thread_init_thread_arena() {
    thread_arena* arena = (thread_arena*)malloc(sizeof(thread_arena));
    arena->cur_block = NULL;
    arena->offset = NULL;
    arena->cur_empty_space = 0;
    return arena;
}

void thread_cleanup_thread_arena(thread_arena* arena) {
    if (arena->cur_block) {
        void* iter = arena->cur_block;
        while (iter) {
            thread_block_header* block_header = (thread_block_header*)iter;
            void* next_block = block_header->next_block;
            global_pool_manager_cleanup_thread_block(iter);
            iter = next_block;
        }
    }
    free(arena);
}

void thread_cleanup_thread_arena_except_current(thread_arena* arena) {
    if(!arena->cur_block)return;
    void* iter = ((thread_block_header*)(arena->cur_block))->next_block;
    ((thread_block_header*)(arena->cur_block))->next_block = NULL;
    while (iter) {
        thread_block_header* block_header = (thread_block_header*)iter;
        void* next_block = block_header->next_block;
        global_pool_manager_cleanup_thread_block(iter);
        iter = next_block;
    }
    //reset current arena
    arena->offset = (char*)(arena->cur_block) + THREAD_BLOCK_HEADER_SIZE;
    arena->cur_empty_space = thread_find_arena_size(arena->cur_block) - THREAD_BLOCK_HEADER_SIZE;
}
