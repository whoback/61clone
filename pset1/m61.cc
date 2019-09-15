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
std::list<meta *> l;

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc must
///    return a unique, newly-allocated pointer value. The allocation
///    request was at location `file`:`line`.

void *m61_malloc(size_t sz, const char *file, long line)
{
    (void)file, (void)line; // avoid uninitialized variable warnings
    // Your code here.
    
    //initialize metadata struct
    meta allocation_metadata;
    allocation_metadata.size = sz;

    // test005 make sure sz requested isn't too large to handle
    if (!sz || sz > 0x10000000000)
    {
        global_stats.nfail++;
        global_stats.fail_size += sz;
        return nullptr;
    }

    //this is a pointer to the requested allocation plus the metadata information
    meta* ptr_to_block;
    ptr_to_block = reinterpret_cast<meta*>(base_malloc(sz + sizeof(meta)));
    //assign pointer to place in memory where allocation_metadata struct lives
    *ptr_to_block = allocation_metadata;
    //add to allocations list
    l.push_back(ptr_to_block);
    //this is a pointer to just the requested allocation which will be returned
    void* ptr_to_alloc;
    ptr_to_alloc = reinterpret_cast<void*>(reinterpret_cast<char*>(ptr_to_block) + sizeof(meta));

    //Stats
    //test003 track actives but we also need to free them
    global_stats.nactive++;
    //test002 track allocation totals
    global_stats.ntotal++;
    //test004 total allocation size
    global_stats.total_size += sz;

    //test006
    global_stats.active_size += sz;

    //test009 and test010 heap_min and heap_max checking
    if(reinterpret_cast<uintptr_t>(ptr_to_alloc) < global_stats.heap_min)
    {
        global_stats.heap_min = reinterpret_cast<uintptr_t>(ptr_to_alloc);
    }
    if (reinterpret_cast<uintptr_t>(ptr_to_alloc) + sz > global_stats.heap_max)
    {
        global_stats.heap_max = reinterpret_cast<uintptr_t>(ptr_to_alloc) + sz;
    }

    return ptr_to_alloc;
}

/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc. If `ptr == NULL`,
///    does nothing. The free was called at location `file`:`line`.

void m61_free(void *ptr, const char *file, long line)
{
    (void)file, (void)line; // avoid uninitialized variable warnings
    // Your code here.
    //can't free nothing so
    if(ptr == nullptr || ptr == NULL)
    {
        return;
    }
    //cast ptr to uintptr_t for math
    meta* ptr_to_metadata = reinterpret_cast<meta*>(reinterpret_cast<uintptr_t>(ptr) - sizeof(meta));
    
    base_free(ptr);
    //test003 freeing an alloc means we need to remove it from our stats
    global_stats.nactive--;
    //test006 make sure to update active allocation size when freeing
    global_stats.active_size -= ptr_to_metadata->size;
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