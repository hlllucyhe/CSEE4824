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
        for (size_t offset = 0; offset < bytes; offset += 64) {
            clflush(lineBuffer + offset);
            clflush(lineBufferCopy + offset);
        }
        clock = clock + (end - start);
        printf("%llu ticks to copy %zuB\n", (end - start), bytes);
        fprintf(out, "%zu,%llu\n", bytes, (end - start));
    }

    printf("took %llu ticks total\n", clock);
}

int main(int ac, char **av) {
    FILE *out = fopen("results.csv", "w");
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
