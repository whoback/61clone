#define M61_DISABLE 1
#include "m61.hh"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <algorithm>
#include <vector>
#define ALIGN_SIZE 8
//track stats
m61_statistics global_stats = {};


//this vector stores every meta struct we've allocated
std::vector<meta> allocations;
/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc must
///    return a unique, newly-allocated pointer value. The allocation
///    request was at location `file`:`line`.

//if sz isn't power of 2 lets fix that

size_t align_help(size_t s)
{
    size_t res = s % ALIGN_SIZE * sizeof(char);

    return res;
}


void *m61_malloc(size_t sz, const char *file, long line)
{
    (void)file, (void)line; // avoid uninitialized variable warnings
    // Your code here.
    // test005 make sure sz requested isn't too large to handle
    if (!sz || sz > 0x10000000000)
    {
        global_stats.nfail++;
        global_stats.fail_size += sz;
        return nullptr;
    }

    //initialize metadata struct
    meta allocation_metadata;
    allocation_metadata.size = sz;
    //active is 1 inactive is 0
    allocation_metadata.active = 1;

    //this is a pointer to the requested allocation plus the metadata information
    meta *ptr_to_block;
    //logging info
    //entire allocation includes requested sz + size_t + size of struct + any padding from odd sz
    ptr_to_block = (meta *)(base_malloc(sz + sizeof(size_t) + sizeof(meta) + align_help(sz)));

    void *ptr_to_payload = (void *)(ptr_to_block + sizeof(meta));

    //test009 and test010 heap_min and heap_max checking
    if (reinterpret_cast<uintptr_t>(ptr_to_payload) < global_stats.heap_min)
    {
        global_stats.heap_min = reinterpret_cast<uintptr_t>(ptr_to_payload);
    }
    if (reinterpret_cast<uintptr_t>(ptr_to_payload) >= global_stats.heap_max)
    {
        global_stats.heap_max = reinterpret_cast<uintptr_t>(ptr_to_payload) + sz;
    }
    //assign pointer to place in memory where allocation_metadata struct lives
    *ptr_to_block = allocation_metadata;
    //add to allocations list

    if (allocations.empty())
    {
        global_stats.heap_min = reinterpret_cast<uintptr_t>(ptr_to_block);
    }
    //test vector
    //add addr to metadata struct
    allocation_metadata.alloc_addr = (uintptr_t)((char *)ptr_to_block + sizeof(meta));
    allocations.push_back(allocation_metadata);

    //Stats
    //test003 track actives but we also need to free them
    global_stats.nactive++;
    //test002 track allocation totals
    global_stats.ntotal++;
    //test004 total allocation size
    global_stats.total_size += sz;

    //test006
    global_stats.active_size += sz;

    return (void *)((char *)ptr_to_block + sizeof(meta));
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
    if (ptr == nullptr || ptr == NULL)
    {
        return;
    }
    //cast ptr to uintptr_t for math
    meta *ptr_to_metadata = reinterpret_cast<meta *>(reinterpret_cast<uintptr_t>(ptr) - sizeof(meta));
    //ptr_to_metadata

    //test016
    if ((uintptr_t)ptr < global_stats.heap_min ||
        (uintptr_t)ptr > global_stats.heap_max)
    {
        printf("MEMORY BUG %s:%li: invalid free of pointer %p, not in heap\n", file, line, ptr);
        return;
    }
    // lambda for searching our vector of allocations
    auto valid_alloc = [](const meta &x) {
        return x.alloc_addr;
    };
    auto r = std::find_if(std::begin(allocations), std::end(allocations), valid_alloc);
    //check to see if allocation is active and we exist in allocations list
    //fixdes test020
    if (ptr_to_metadata->active != 1 && r != std::end(allocations))
    {
        printf("MEMORY BUG %s:%li: invalid free of pointer %p, double free\n", file, line, ptr);
        return;
    }

    //check if ptr passed is actually an addr we have
    //storing ptr addresses we've used in a vector
    
    if (r != std::end(allocations))
    {
        // if not in allocations means we've freed so don't double free
        //flip allocation active state
        ptr_to_metadata->active = 0;
        //test003 freeing an alloc means we need to remove it from our stats
        global_stats.nactive--;
        //test006 make sure to update active allocation size when freeing
        global_stats.active_size -= ptr_to_metadata->size;
        return;
        //ptr is in our allocations vector
    }
    else
    {
        printf("MEMORY BUG %s:%li: invalid free of pointer %p, not allocated\n", file, line, ptr);
        return;
    }

    //flip allocation active state to 0 = not active
    ptr_to_metadata->active = 0;
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

    if ((nmemb * sz) / nmemb != sz)
    {
        global_stats.nfail++;
        return nullptr;
    }
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