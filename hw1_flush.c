#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPEAT 100000
#define CACHELINE 64

inline void clflush(volatile void *p) {
    asm volatile("clflush (%0)" :: "r"(p));
}

inline void clflush_bytes(volatile void *p, size_t bytes) {
    uintptr_t start = (uintptr_t)p;
    uintptr_t aligned_start = start & ~(uintptr_t)(CACHELINE - 1);
    uintptr_t end = start + bytes;
    uintptr_t aligned_end = (end + CACHELINE - 1) & ~(uintptr_t)(CACHELINE - 1);

    for (uintptr_t addr = aligned_start; addr < aligned_end; addr += CACHELINE) {
        asm volatile("clflush (%0)" :: "r"((const void*)addr));
    }
    asm volatile("mfence" ::: "memory");
}

inline uint64_t rdtsc() {
    unsigned long a, d;
    asm volatile("rdtsc" : "=a"(a), "=d"(d));
    return a | ((uint64_t)d << 32);
}

inline void memtest(size_t bytes, FILE *out) {
    uint64_t start, end, clock;
    char *lineBuffer = (char *)malloc(bytes);
    char *lineBufferCopy = (char *)malloc(bytes);

    for (int i = 0; i < bytes; i++) {
        lineBuffer[i] = '1';
    }

    clock = 0;
    for (long rep = 0; rep < REPEAT; rep++) {
        start = rdtsc();
        memcpy(lineBufferCopy, lineBuffer, bytes);
        end = rdtsc();
        clflush_bytes(lineBuffer, bytes);
        clflush_bytes(lineBufferCopy, bytes);
        clock = clock + (end - start);
        //printf("%llu ticks to copy %zuB\n", (end - start), bytes);
        fprintf(out, "%zu,%llu\n", bytes, (end - start));
    }

    printf("took %llu ticks total\n", clock);
    free(lineBuffer);
    free(lineBufferCopy);
}

int main(int ac, char **av) {
    FILE *out = fopen("results2.csv", "w");
    fprintf(out, "Size(Bytes),Time(Ticks)\n");
    printf("------------------------------\n");
    const int exps[] = {6,7,8,9,10,11,12,13,14,15,16,20,21};
    for (int i = 0; i< 13; i++) {
        size_t bytes = (size_t) 1 << exps[i];
        memtest(bytes, out);
        printf("------------------------------\n");
    }
    fclose(out);
    return 0;
}
