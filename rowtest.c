#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>   // for __rdtscp, _mm_clflush

#define REPEAT 1000000
#define CACHELINE 64
#define ROW_SIZE (8*1024)   // row size = 32KB

inline void clflush(volatile void *p) {
    _mm_clflush(p);
    _mm_mfence();
}

inline uint64_t rdtsc() {
    unsigned int aux;
    _mm_lfence();
    return __rdtscp(&aux);
}

//one test same-row vs diff-row
void rowtest() {
    char *buf;
    if (posix_memalign((void**)&buf, ROW_SIZE, ROW_SIZE*4)) {
        perror("posix_memalign");
        exit(1);
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

        uint64_t t0 = rdtsc();
        tmp = *addr1;
        uint64_t t1 = rdtsc();
        sum_first_same += (t1 - t0);

        clflush(addr2_same);
        t0 = rdtsc();
        tmp = *addr2_same;
        t1 = rdtsc();
        sum_second_same += (t1 - t0);

        // ---- diff row case ----
        clflush(addr1);
        clflush(addr2_diff);

        t0 = rdtsc();
        tmp = *addr1;
        t1 = rdtsc();
        sum_first_diff += (t1 - t0);

        clflush(addr2_diff);
        t0 = rdtsc();
        tmp = *addr2_diff;
        t1 = rdtsc();
        sum_second_diff += (t1 - t0);
    }

    printf("Average over %d runs:\n", REPEAT);
    printf("Same-row First access:   %lu cycles\n", sum_first_same / REPEAT);
    printf("Same-row Second access:  %lu cycles\n", sum_second_same / REPEAT);
    printf("Diff-row First access:   %lu cycles\n", sum_first_diff / REPEAT);
    printf("Diff-row Second access:  %lu cycles\n", sum_second_diff / REPEAT);

    free(buf);
}

int main() {
    printf("===== Row Buffer Policy Test =====\n");
    rowtest();
    return 0;
}