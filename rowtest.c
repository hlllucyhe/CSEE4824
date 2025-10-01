#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPEAT 1000000

inline void clflush(volatile void *p) {
    asm volatile("clflush (%0)" :: "r"(p));
}

inline uint64_t rdtsc() {
    unsigned long a, d;
    asm volatile("rdtsc" : "=a"(a), "=d"(d));
    return a | ((uint64_t)d << 32);
}

long int rep;

inline void rowtest() {
    uint64_t start, end;
    uint64_t clock1 = 0, clock2 = 0, clock3 = 0;

    size_t bufSize = 64 * 1024;  // 64 KB
    char *row1 = (char *)malloc(bufSize);
    char *row2 = (char *)malloc(bufSize);

    // 初始化
    memset(row1, 'A', bufSize);
    memset(row2, 'B', bufSize);

    volatile char tmp;

    for (rep = 0; rep < REPEAT; rep++) {
        // Step1: row1
        start = rdtsc();
        tmp = row1[0];
        end = rdtsc();
        clock1 += (end - start);

        // Step2: row2 （diff row)
        start = rdtsc();
        tmp = row2[0];
        end = rdtsc();
        clock2 += (end - start);

        // Step3: repeat access row2 (same row)
        start = rdtsc();
        tmp = row2[64]; 
        end = rdtsc();
        clock3 += (end - start);

        clflush(row1);
        clflush(row2);
    }

    printf("Avg access row1: %llu cycles\n", clock1 / REPEAT);
    printf("Avg access row2 (first): %llu cycles\n", clock2 / REPEAT);
    printf("Avg access row2 (second): %llu cycles\n", clock3 / REPEAT);
}

int main() {
    printf("===== Row Buffer Policy Test =====\n");
    rowtest();
    return 0;
}