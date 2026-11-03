typedef struct {
    void* block;
    void* offset;
    int cur_empty_space;
} thread_arena;

void* thread_arena_alloc(thread_arena* arena, int size);
thread_arena* thread_init_thread_arena();
void thread_cleanup_thread_arena(thread_arena* arena);
void thread_cleanup_thread_arena_except_current(thread_arena *arena);


