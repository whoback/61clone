#define M61_DISABLE 1
#include "m61.hh"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>

//track stats
m61_statistics global_stats = {0,0,0,0,0,0,0,0};

//STL list to hold ptrs to metadata structs
std::list<meta *> metadata_list;

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc must
///    return a unique, newly-allocated pointer value. The allocation
///    request was at location `file`:`line`.

void *m61_malloc(size_t sz, const char *file, long line)
{
    (void)file, (void)line; // avoid uninitialized variable warnings
    // Your code here.
    void* ptr_to_base_malloc;
    //initialize metadata struct
    meta allocation_metadata;
    allocation_metadata.size = sz;
    unsigned int size_of_metadata_struct = sizeof(meta);
    
    

    // test005 make sure sz requested isn't too large to handle
    if (!sz || sz > 0x10000000000)
    {
        global_stats.nfail++;
        global_stats.fail_size += sz;
        return nullptr;
    }
    ptr_to_base_malloc = base_malloc(sz);
    //test003 track actives but we also need to free them
    global_stats.nactive++;
    //test002 track allocation totals
    global_stats.ntotal++;
    //test004 total allocation size
    global_stats.total_size += sz;

    return ptr_to_base_malloc;
}

/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc. If `ptr == NULL`,
///    does nothing. The free was called at location `file`:`line`.

void m61_free(void *ptr, const char *file, long line)
{
    (void)file, (void)line; // avoid uninitialized variable warnings
    // Your code here.
    base_free(ptr);
    //test003 freeing an alloc means we need to remove it from our stats
    global_stats.nactive--;
    //test006 make sure to update active allocation size when freeing
    global_stats.active_size -= 
}

/// m61_calloc(nmemb, sz, file, line)
///    Return a pointer to newly-allocated dynamic memory big enough to
///    hold an array of `nmemb` elements of `sz` bytes each. If `sz == 0`,
///    then must return a unique, newly-allocated pointer value. Returned
///    memory should be initialized to zero. The allocation request was at
///    location `file`:`line`.

void *m61_calloc(size_t nmemb, size_t sz, const char *file, long line)
{
    // Your code here (to fix test014).
    void *ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr)
    {
        memset(ptr, 0, nmemb * sz);
    }
    return ptr;
}

/// m61_get_statistics(stats)
///    Store the current memory statistics in `*stats`.

void m61_get_statistics(m61_statistics *stats)
{
    // Stub: set all statistics to enormous numbers
    memset(stats, 255, sizeof(m61_statistics));
    // Your code here.
    //test001 - init stats tracking
    *stats = global_stats;
}

/// m61_print_statistics()
///    Print the current memory statistics.

void m61_print_statistics()
{
    m61_statistics stats;
    m61_get_statistics(&stats);

    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}

/// m61_print_leak_report()
///    Print a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_print_leak_report()
{
    // Your code here.
}

/// m61_print_heavy_hitter_report()
///    Print a report of heavily-used allocation locations.

void m61_print_heavy_hitter_report()
{
    // Your heavy-hitters code here
}