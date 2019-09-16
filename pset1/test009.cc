#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// heap_min and heap_max checking, simple case.

int main() {
    char* p = (char*) malloc(10);
    m61_statistics stat;
    m61_get_statistics(&stat);
    printf("heap_min: %lu\n heap_max: %lu\n", stat.heap_min, stat.heap_max);
    assert((uintptr_t) p >= stat.heap_min);
    assert((uintptr_t) p + 10 <= stat.heap_max);
}
