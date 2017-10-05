#include <stdint.h>

void sort(
    void *base, uint64_t num, uint64_t size,
    int (*cmp_func)(const void *, const void *),
    void (*swap_func)(void *, void *, int size));
