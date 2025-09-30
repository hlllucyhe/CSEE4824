#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>

#define CACHELINE 64
#define ROW_SIZE (8*1024)   // row buffer = 8 kB

static inline void clflush(void *p) {
    _mm_clflush(p);
    _mm_mfence();
}

static inline uint64_t tsc_begin(void) {
    unsigned int aux;
    _mm_lfence();
    return __rdtscp(&aux);
}

static inline uint64_t tsc_end(void) {
    unsigned int aux;
    uint64_t t = __rdtscp(&aux);
    _mm_lfence();
    return t;
}

int main(void) {
    // 分配对齐内存
    char *buf;
    if (posix_memalign((void**)&buf, ROW_SIZE, ROW_SIZE*2)) {
        perror("posix_memalign");
        return 1;
    }

    volatile char tmp;
    char *addr1       = buf;
    char *addr2_same  = buf + 64;         // same row
    char *addr2_diff  = buf + ROW_SIZE;   // diff row

    // Case 1: same row
    clflush(addr1);
    clflush(addr2_same);

    uint64_t t0 = tsc_begin();
    tmp = *addr1;
    uint64_t t1 = tsc_end();
    printf("First access (same-row case): %lu cycles\n", t1 - t0);

    t0 = tsc_begin();
    tmp = *addr2_same;
    t1 = tsc_end();
    printf("Second access (same-row): %lu cycles\n", t1 - t0);

    // Case 2: diff row
    clflush(addr1);
    clflush(addr2_diff);

    t0 = tsc_begin();
    tmp = *addr1;
    t1 = tsc_end();
    printf("First access (diff-row case): %lu cycles\n", t1 - t0);

    t0 = tsc_begin();
    tmp = *addr2_diff;
    t1 = tsc_end();
    printf("Second access (diff-row): %lu cycles\n", t1 - t0);

    free(buf);
    return 0;
}