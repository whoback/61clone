#define M61_DISABLE 1
#include "m61.hh"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#define MAGIC_META_ID 4206969

m61_statistics global_stats = {0, 0, 0, 0, 0, 0, 0, 0};
//this is our free list
struct header global_base = {0, 1337, 0, nullptr, nullptr, NULL, 0};
/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc must
///    return a unique, newly-allocated pointer value. The allocation
///    request was at location `file`:`line`.

void *m61_malloc(size_t sz, const char *file, long line)
{
    (void)file, (void)line; // avoid uninitialized variable warnings
    // Your code here.

    //check to make sure size isn't too big
    //taken from the test verbatim not sure about that feels yucky but works
    if (sz >= ((size_t)-1 - 150))
    {
        //if it is fail and update stats accordingly
        global_stats.nfail++;         //number of total failed allocs
        global_stats.fail_size += sz; //running total of failed alloc request size in bytes
        return nullptr;
    }

    //actual amount to return
    size_t rounded_sz = (sz + sizeof(header) + sizeof(size_t) + 8);

    struct header metadata = {};
    metadata.size = sz;        //originial requested size to be used by free
    metadata.is_active = 1337; //this data is currently malloced
    metadata.metadata_id = MAGIC_META_ID;
    //updates for leak report
    metadata.file = file;
    metadata.line = line;

    //this inculdes meta + payload (user requested sz)
    header *ptr_to_allocation = (header *)base_malloc(rounded_sz);
    *ptr_to_allocation = metadata;
    //attach to the list
    ptr_to_allocation->ptr_to_next = global_base.ptr_to_next;
    ptr_to_allocation->ptr_to_last = &global_base;
    //circle the list around
    if (global_base.ptr_to_next != nullptr)
    {
        global_base.ptr_to_next->ptr_to_last = ptr_to_allocation;
    }
    global_base.ptr_to_next = ptr_to_allocation;

    //pointer to return
    //the actual requested data
    void *ptr = (void *)((char *)ptr_to_allocation + sizeof(header));

    //trailer help
    //this is the ptr to requested data + the size that 
    //gets us to the end of the total allocation to add
    char *ptr_to_trailer = (char *)ptr + sz;
    *ptr_to_trailer = '@';

    //heap min and max testing
    if (global_stats.heap_min == 0)
    {
        global_stats.heap_min = (uintptr_t)ptr;
    }
    if (global_stats.heap_max <= (uintptr_t)ptr)
    {
        global_stats.heap_max = (uintptr_t)ptr + sz;
    }

    if (global_stats.heap_min > (uintptr_t)ptr)
    {
        global_stats.heap_min = (uintptr_t)ptr;
    }

    //update stats on sucess
    global_stats.ntotal++;          //number of total allocations
    global_stats.total_size += sz;  //total number of bytes in successful allocs
    global_stats.nactive++;         //num of active allocs
    global_stats.active_size += sz; //active minus freed allocation sizes in bytes
    return ptr;
}

/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc. If `ptr == NULL`,
///    does nothing. The free was called at location `file`:`line`.

void m61_free(void *ptr, const char *file, long line)
{
    (void)file, (void)line; // avoid uninitialized variable warnings
    // Your code here.
    //null pointers are freeable
    if (ptr == nullptr)
    {
        return;
    }

    //are we in el heap?
    if ((uintptr_t)ptr < global_stats.heap_min || (uintptr_t)ptr > global_stats.heap_max)
    {
        printf("MEMORY BUG %s:%li: invalid free of pointer %p, not in heap\n", file, line, ptr);
        return;
    }
    //need to subtract struct to get to metadata
    header *ptr_to_meta = (header *)((char *)ptr - sizeof(header));

    //bitwise AND operation (0000 0000 0000 0111) checking to see if our ptr address is div by 8
    //if not then we know we're dealing with an unallocated call
    if ((((uintptr_t)ptr & 7) != 0) || (ptr_to_meta->metadata_id != MAGIC_META_ID))
    {
        printf("MEMORY BUG: %s:%li: invalid free of pointer %p, not allocated\n", file, line, ptr);
        //check to see if ptr is trying to free inside another heap block
        header *bad_ptr = global_base.ptr_to_next;
        //keep checking until end of list
        while (bad_ptr != nullptr)
        {
            if (ptr >= bad_ptr && ptr <= (char *)bad_ptr + sizeof(header) + bad_ptr->size)
            {
                size_t offset = (char *)ptr - ((char *)bad_ptr + sizeof(header));
                printf("  %s:%li: %p is %zu bytes inside a %lu byte region allocated here\n",
                       file, bad_ptr->line, ptr, offset, bad_ptr->size);
                break;
            }
            //update bad_ptr to the next list item
            bad_ptr = bad_ptr->ptr_to_next;
        }
        return;
    }
    //check to see if we've already freed
    if (ptr_to_meta->is_active == 8008 && ptr_to_meta->metadata_id == MAGIC_META_ID)
    {
        printf("MEMORY BUG: invalid free of pointer %p, double free\n", ptr);
        return;
    }
    //test0023
    if (ptr_to_meta->ptr_to_next != nullptr)
    {
        if (ptr_to_meta->ptr_to_next->ptr_to_last != ptr_to_meta)
        {
            printf("MEMORY BUG: %s:%li: invalid free of pointer %p, not allocated\n", file, line, ptr);
            return;
        }
    }

    if (ptr_to_meta->ptr_to_last != nullptr)
    {
        if (ptr_to_meta->ptr_to_last->ptr_to_next != ptr_to_meta)
        {
            printf("MEMORY BUG: %s: %li: invalid free of pointer %p, not allocated\n", file, line, ptr);
            return;
        }
    }

    //attempt to catch wild - taken from class answer at begining of last lecture thanks James?
    char *ptr_to_trailer = (char *)ptr + ptr_to_meta->size;
    if (*ptr_to_trailer != '@')
    {
        //we know our mem has been modified
        printf("MEMORY BUG: %s:%li: detected wild write during free of pointer %p\n", file, line, ptr);
        return;
    }

    //list updoots
    if (ptr_to_meta->ptr_to_next != nullptr)
    {
        (ptr_to_meta->ptr_to_next)->ptr_to_last = ptr_to_meta->ptr_to_last;
    }
    if (ptr_to_meta->ptr_to_last != nullptr)
    {
        (ptr_to_meta->ptr_to_last)->ptr_to_next = ptr_to_meta->ptr_to_next;
    }
    else
    {
        global_base.ptr_to_next = ptr_to_meta->ptr_to_next;
    }

    //stat updoots
    global_stats.nactive--;
    global_stats.active_size -= (ptr_to_meta->size);
    ptr_to_meta->is_active = 8008;

    //free it
    base_free(ptr_to_meta);
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
    if ((nmemb * sz / nmemb) != sz)
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
    //we can itr over our list and print info from it
    header *ptr_to_next = global_base.ptr_to_next;
    //keep going until end of list
    while (ptr_to_next != nullptr)
    {
        //get info from struct
        const char *file = ptr_to_next->file;
        long line = ptr_to_next->line;
        size_t sz = ptr_to_next->size;
        //print info
        printf("LEAK CHECK: %s:%li: allocated object %p with size %zu\n", file, line, (header *)((char *)ptr_to_next + sizeof(header)), sz);
        //update to next struct
        ptr_to_next = ptr_to_next->ptr_to_next;
    }
    return;
}

/// m61_print_heavy_hitter_report()
///    Print a report of heavily-used allocation locations.

void m61_print_heavy_hitter_report()
{
    // Your heavy-hitters code here
    //TODO
    //track usage
    //only have to track orig request sz NOT METADATA
    //20% or more use total_size to help calc
    //sort in decending order ie higher lines first
    //print just like other reports using file and line numbers
}
