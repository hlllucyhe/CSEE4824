#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>

#define CACHELINE 64
#define ROW_SIZE  (8*1024)   // row buffer = 8KB
#define REPEAT    100        // repeat

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
    char *buf;
    if (posix_memalign((void**)&buf, ROW_SIZE, ROW_SIZE*2)) {
        perror("posix_memalign");
        return 1;
    }

    volatile char tmp;
    char *addr1      = buf;
    char *addr2_same = buf + 64;         // same row
    char *addr2_diff = buf + ROW_SIZE;   // diff row

    uint64_t sum_first_same = 0, sum_second_same = 0;
    uint64_t sum_first_diff = 0, sum_second_diff = 0;

    for (int r = 0; r < REPEAT; r++) {
        // ---- same row case ----
        clflush(addr1);
        clflush(addr2_same);

        uint64_t t0 = tsc_begin();
        tmp = *addr1;
        uint64_t t1 = tsc_end();
        sum_first_same += (t1 - t0);

        clflush(addr2_same); // avoid effect of cache 
        t0 = tsc_begin();
        tmp = *addr2_same;
        t1 = tsc_end();
        sum_second_same += (t1 - t0);

        // ---- diff row case ----
        clflush(addr1);
        clflush(addr2_diff);

        t0 = tsc_begin();
        tmp = *addr1;
        t1 = tsc_end();
        sum_first_diff += (t1 - t0);

        clflush(addr2_diff); // avoid cache effect
        t0 = tsc_begin();
        tmp = *addr2_diff;
        t1 = tsc_end();
        sum_second_diff += (t1 - t0);
    }

    printf("Average over %d runs:\n", REPEAT);
    printf("Same-row First access:   %lu cycles\n", sum_first_same / REPEAT);
    printf("Same-row Second access:  %lu cycles\n", sum_second_same / REPEAT);
    printf("Diff-row First access:   %lu cycles\n", sum_first_diff / REPEAT);
    printf("Diff-row Second access:  %lu cycles\n", sum_second_diff / REPEAT);

    free(buf);
    return 0;
}