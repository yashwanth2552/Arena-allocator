#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/mman.h>
#include "global_pool_manager.h"

extern global_pool_manager* manager;

void global_pool_manager_update_free_list_bookkeeping(block_header* header) {
    int size = header->size;
    int two_power = size/PAGE_SIZE;
    int size_index = __builtin_ctz(two_power);
    manager->free_list_bitmask |= (1 << size_index);
    free_list_node* node = (free_list_node*)malloc(sizeof(free_list_node));
    node->header = header;
    node->next = manager->size_based_free_list[size_index];
    manager->size_based_free_list[size_index] = node;
}

bool global_pool_manager_validate_arena_ptr(void* arena) {
    block_header* header = (block_header*)((char*)arena - HEADER_SIZE);
    if (header->magic != MAGIC) return false;
    return true;
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
    while (size > (1<<best_possible_index)*PAGE_SIZE) {
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
        void* raw_block = mmap(NULL, HEADER_SIZE+ ((1<<best_possible_index)*PAGE_SIZE), PROT_READ | PROT_WRITE, 
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (raw_block == MAP_FAILED) {
            pthread_mutex_unlock(manager->global_lock);
            return NULL;
        }
        block_header* header = (block_header*)raw_block;
        header->size = ((1<<best_possible_index)*PAGE_SIZE);
        header->is_free = false;
        header->owner_thread_id = cur_thread_id;
        header->next = manager->head_block_list;
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
    free_list_node* node = manager->size_based_free_list[first_set_bit];
    block_header* header = node->header;
    header->is_free = false;
    header->owner_thread_id = cur_thread_id;
    if (node->next == NULL) {
        manager->size_based_free_list[first_set_bit] = NULL;
        manager->free_list_bitmask &= (~(1 << first_set_bit));
    } else {
        manager->size_based_free_list[first_set_bit] = node->next;
    }
    free(node);
    pthread_mutex_unlock(manager->global_lock);
    return (char*)header + HEADER_SIZE;
}

void global_pool_manager_thread_arena_cleanup(){
    pthread_t cur_thread = pthread_self();
    /*
     1)acquire the mutex
     2)traverse the block list
     3)check if block's owner_thread_id = cur_thread and !is_free, if yes then mark it free,
        and make corresponding changes in size_based_free_list and bitmask.
     4)release the mutex
     */
    pthread_mutex_lock(manager->global_lock);
    block_header* cur = manager->head_block_list;
    while (cur) {
        if (!cur->is_free && cur->owner_thread_id==cur_thread) {
            cur->is_free = true;
            global_pool_manager_update_free_list_bookkeeping(cur);
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(manager->global_lock);
    return ;
}

bool global_pool_manager_thread_arena_cleanup_except_current(void* current_arena) {
    //check if current_arena is valid arena ptr
    if (!global_pool_manager_validate_arena_ptr(current_arena)) {
        printf("Invalid arena ptr passed!!\n");
        return false;
    }
    pthread_mutex_lock(manager->global_lock);
    pthread_t cur_thread = pthread_self();
    block_header* cur = manager->head_block_list;
    block_header* current_arena_header = (char*)current_arena - HEADER_SIZE;
    while (cur) {
        if (!cur->is_free && cur->owner_thread_id == cur_thread && cur != current_arena_header) {
            cur->is_free = true;
            global_pool_manager_update_free_list_bookkeeping(cur);
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(manager->global_lock);
    return true;
}
