#include "thread_arena.h"
#include "global_pool_manager.h"
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

int thread_find_arena_size(void* block) {
    block_header* header = (block_header*)((char*)block - HEADER_SIZE);
    return header->size;
}

void* thread_arena_alloc(thread_arena* arena, int size) {
    //round-off size for better memory alignment
    int aligned_size = (size + 7) & ~7;
    if (arena->cur_empty_space < aligned_size) {
        arena->block = global_pool_manager_arena_alloc(aligned_size);
        arena->offset = arena->block;
        arena->cur_empty_space = thread_find_arena_size(arena->block);
    }
    void* ret = arena->offset;
    arena->offset = (char*)arena->offset + aligned_size;
    arena->cur_empty_space -= aligned_size;
    return ret;
}

thread_arena* thread_init_thread_arena() {
    thread_arena* arena = (thread_arena*)malloc(sizeof(thread_arena));
    arena->block = NULL;
    arena->offset = NULL;
    arena->cur_empty_space = 0;
    return arena;
}

void thread_cleanup_thread_arena(thread_arena* arena) {
    global_pool_manager_thread_arena_cleanup();
    free(arena);
}

void thread_cleanup_thread_arena_except_current(thread_arena* arena) {
   if (!global_pool_manager_thread_arena_cleanup_except_current(arena->block)) {
       printf("Failed to clean thread arena except current\n");
       return ;
   }
    arena->offset = arena->block;
    arena->cur_empty_space = thread_find_arena_size(arena->block);
}
