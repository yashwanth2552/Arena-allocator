#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_ALLOWED_SIZES 10
#define MAX_ALLOWED_SIZE (1 << 30)
#define MIN_ALLOC_SIZE (1 << 21)
#define MAGIC 0x4D414C43

typedef struct block_header {
    struct block_header* next_global;
    struct block_header* next_free_list;
    bool is_free;
    int size;
    pthread_t owner_thread_id;
    int magic;
} block_header;
#define HEADER_SIZE sizeof(block_header)

typedef struct {
    block_header* size_based_free_list[MAX_ALLOWED_SIZES];
    block_header* head_block_list;
    int free_list_bitmask;
    pthread_mutex_t* global_lock;
} global_pool_manager;

global_pool_manager* init_global_pool_manager(pthread_mutex_t* global_lock);
void cleanup_global_pool_manager();
void* global_pool_manager_arena_alloc(int size);
void global_pool_manager_cleanup_thread_block(void* arena);
