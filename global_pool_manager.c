#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/mman.h>
#include "global_pool_manager.h"

extern global_pool_manager* manager;

void global_pool_manager_update_free_list_bookkeeping(block_header* header) {
    int size = header->size;
    int two_power = size/MIN_ALLOC_SIZE;
    int size_index = __builtin_ctz(two_power);
    manager->free_list_bitmask |= (1 << size_index);
    header->next_free_list = manager->size_based_free_list[size_index];
    manager->size_based_free_list[size_index] = header;
}

bool global_pool_manager_validate_arena_ptr(void* arena) {
    block_header* header = (block_header*)((char*)arena - HEADER_SIZE);
    if (header->magic != MAGIC) return false;
    return true;
}

void cleanup_global_pool_manager() {
    if (!manager) return ;
    pthread_mutex_destroy(manager->global_lock);
    manager->global_lock = NULL;

    block_header* iter = manager->head_block_list;
    while (iter) {
        block_header* next_block = iter->next_global;
        int allocated_size = iter->size + HEADER_SIZE;
        munmap(iter, allocated_size);
        iter = next_block;
    }
    free(manager);
    manager = NULL;
}

global_pool_manager* init_global_pool_manager(pthread_mutex_t* global_lock) {
    global_pool_manager* manager = (global_pool_manager*)malloc(sizeof(global_pool_manager));
    if (manager == NULL) {
        return NULL;
    }
    manager->head_block_list = NULL;
    manager->free_list_bitmask = 0;
    manager->global_lock = global_lock;
    for (int i=0; i < MAX_ALLOWED_SIZES; i++) {
        manager->size_based_free_list[i] = NULL;
    }
    return manager;
}

void* global_pool_manager_arena_alloc(int size) {
    if (size > MAX_ALLOWED_SIZE) return NULL;
    int best_possible_index = 0;
    pthread_t cur_thread_id = pthread_self();
    while (size > (1<<best_possible_index)*MIN_ALLOC_SIZE) {
        best_possible_index++;
    }
    int check_bitmask = ~((1<<best_possible_index)-1);

    pthread_mutex_lock(manager->global_lock);

    int res_bitmask = check_bitmask & manager->free_list_bitmask;
    if (res_bitmask == 0) {
        /*
         1)allocate the size
         2)insert it at the head of the main block list
         3)return the address of the block
         */
        void* raw_block = mmap(NULL, HEADER_SIZE+ ((1<<best_possible_index)*MIN_ALLOC_SIZE), PROT_READ | PROT_WRITE, 
                                MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        if (raw_block == MAP_FAILED) {
            pthread_mutex_unlock(manager->global_lock);
            return NULL;
        }
        block_header* header = (block_header*)raw_block;
        header->size = ((1<<best_possible_index)*MIN_ALLOC_SIZE);
        header->is_free = false;
        header->owner_thread_id = cur_thread_id;
        header->next_global = manager->head_block_list;
        header->next_free_list = NULL;
        header->magic = MAGIC;
        manager->head_block_list = header;
        pthread_mutex_unlock(manager->global_lock);
        return (char*)raw_block + HEADER_SIZE;
    }
    int first_set_bit = __builtin_ctz(res_bitmask);
    /*
     1)We need to allocate the block at the head of free_list
     2)If this is the only node in free_list set that particular index to NULL, and change the bitmask.
     3)change the header's metadata
     */
    block_header* header = manager->size_based_free_list[first_set_bit];;
    header->is_free = false;
    header->owner_thread_id = cur_thread_id;
    if (header->next_free_list == NULL) {
        manager->size_based_free_list[first_set_bit] = NULL;
        manager->free_list_bitmask &= (~(1 << first_set_bit));
    } else {
        manager->size_based_free_list[first_set_bit] = header->next_free_list;
    }
    pthread_mutex_unlock(manager->global_lock);
    return (char*)header + HEADER_SIZE;
}

void global_pool_manager_cleanup_thread_block(void* arena) {
    if (!global_pool_manager_validate_arena_ptr(arena)) {
        printf("Invalid arena ptr passed!!\n");
        return;
    }
    pthread_mutex_lock(manager->global_lock);
    block_header* header = (block_header*)((char*)arena - HEADER_SIZE);
    header->is_free = true;
    global_pool_manager_update_free_list_bookkeeping(header);
    pthread_mutex_unlock(manager->global_lock);
}
