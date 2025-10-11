#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define REPEAT 1000000
#define ROW_SIZE 8192   // row size = 8 kB

inline void clflush(volatile void *p) {
    asm volatile("clflush (%0)" :: "r"(p));
}

inline uint64_t rdtsc() {
    unsigned long a, d;
    asm volatile("rdtsc" : "=a"(a), "=d"(d));
    return a | ((uint64_t)d << 32);
}

long int rep;

void rowtest() {
    uint64_t start, end;
    uint64_t clock1 = 0, clock2 = 0, clock3 = 0;

    char *buf;
    if (posix_memalign((void**)&buf, ROW_SIZE, 2 * ROW_SIZE)) {
        perror("posix_memalign");
        exit(1);
    }

    char *row1 = buf;
    char *row2 = buf + ROW_SIZE;

    memset(row1, 'A', ROW_SIZE);
    memset(row2, 'B', ROW_SIZE);

    volatile char tmp;

    for (rep = 0; rep < REPEAT; rep++) {
        // Step1: row1
        start = rdtsc();
        tmp = row1[0];
        end = rdtsc();
        clock1 += (end - start);

        // Step2: row2（diff row）
        start = rdtsc();
        tmp = row2[0];
        end = rdtsc();
        clock2 += (end - start);

        // Step3: row2（same row again）
        start = rdtsc();
        tmp = row2[64];
        end = rdtsc();
        clock3 += (end - start);

        clflush(row1);
        clflush(row2);
    }

    printf("===== Row Buffer Policy Test =====\n");
    printf("Avg access row1: %" PRIu64 " cycles\n", clock1 / REPEAT);
    printf("Avg access row2 (first): %" PRIu64 " cycles\n", clock2 / REPEAT);
    printf("Avg access row2 (second): %" PRIu64 " cycles\n", clock3 / REPEAT);
}

int main() {
    rowtest();
    return 0;
}