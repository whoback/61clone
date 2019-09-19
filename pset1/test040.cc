#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Realloc test stub

int main()
{
     void *ptr = malloc(2001);
     header *ptr_to_meta = (header *)((char *)ptr - sizeof(header));
     assert(ptr_to_meta->size == 2001);
     assert(ptr_to_meta->is_active == 1337);
     
     free(ptr);
     assert(ptr_to_meta->is_active == 8008);
     
     //nullptr test
     //this should behave like malloc(sz, file, line)
     void* nptr = nullptr;
     void* result = realloc(nptr, 100);
     header* ptr_to_res_meta = (header*)((char*)result - sizeof(header));
     assert(ptr_to_res_meta->size == 100);

     //size = 0 test
     //this should behave like free(ptr, file, line)

     void* ptr_to_free = malloc(99);
     header* ptr_to_free_meta = (header*)((char*)ptr_to_free - sizeof(header));
     assert(ptr_to_free_meta->size == 99);
     assert(ptr_to_free_meta->is_active == 1337);
     void* result2 = realloc(ptr_to_free, 0);
     header *ptr_to_res2_meta = (header *)((char *)result2- sizeof(header));
     assert(ptr_to_res2_meta->is_active == 8008);

     //realloc from 5 to 10
     void* og = malloc(5);
     header* ptr_to_og_meta = (header*)((char*)og - sizeof(header));
     void* reog = realloc(og, 10);
     header *ptr_to_reog_meta = (header *)((char *)reog - sizeof(header));
     //new size should be 10
     assert(ptr_to_reog_meta->size == 10);
     //should free old pointer
     assert(ptr_to_og_meta->is_active == 8008);    
    
    // m61_print_statistics();
    
}