
#define MAX_ALLOWED_SIZES 10
#define MAX_ALLOWED_SIZE (1 << 21)
#define PAGE_SIZE (1 << 12)
#define MAGIC 0x4D414C43

typedef struct block_header {
    struct block_header* next;
    bool is_free;
    int size;
    pthread_t owner_thread_id;
    int magic;
} block_header;
#define HEADER_SIZE sizeof(block_header)

typedef struct free_list_node {
    struct free_list_node* next;
    block_header* header;
} free_list_node;

typedef struct {
    free_list_node* size_based_free_list[MAX_ALLOWED_SIZES];
    block_header* head_block_list;
    int free_list_bitmask;
    pthread_mutex_t* global_lock;
} global_pool_manager;

void init_global_pool_manager();
void global_pool_manager_thread_arena_cleanup();
bool global_pool_manager_thread_arena_cleanup_except_current(void* current_arena);
