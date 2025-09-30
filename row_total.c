#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>   // for __rdtscp, _mm_clflush

#define REPEAT 1000000
#define ROW_SIZE (8*1024)   // row size = 8 kB

inline void clflush(volatile void *p) {
    _mm_clflush(p);
    _mm_mfence();
}

inline uint64_t rdtsc() {
    unsigned int aux;
    _mm_lfence();
    return __rdtscp(&aux);
}

void rowtest() {
    char *buf;
    if (posix_memalign((void**)&buf, ROW_SIZE, ROW_SIZE*8)) {
        perror("posix_memalign");
        exit(1);
    }

    volatile char tmp;
    char *addr1      = buf;
    char *addr2_same = buf + 64;         // same row
    char *addr2_diff = buf + ROW_SIZE;   // diff row

    uint64_t sum_same_total = 0;
    uint64_t sum_diff_total = 0;

    for (int r = 0; r < REPEAT; r++) {
        // ---- same row case ----
        clflush(addr1);
        clflush(addr2_same);

        uint64_t t0 = rdtsc();
        tmp = *addr1;         // first
        tmp = *addr2_same;    // second (same row)
        uint64_t t1 = rdtsc();
        sum_same_total += (t1 - t0);

        // ---- diff row case ----
        clflush(addr1);
        clflush(addr2_diff);

        t0 = rdtsc();
        tmp = *addr1;         // first
        tmp = *addr2_diff;    // second (diff row)
        t1 = rdtsc();
        sum_diff_total += (t1 - t0);
    }

    printf("Average over %d runs:\n", REPEAT);
    printf("Same-row total (2 accesses): %lu cycles\n", sum_same_total / REPEAT);
    printf("Diff-row total (2 accesses): %lu cycles\n", sum_diff_total / REPEAT);

    free(buf);
}

int main() {
    printf("===== Row Buffer Policy Test =====\n");
    rowtest();
    return 0;
}