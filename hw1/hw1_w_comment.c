#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <x86intrin.h>   // for _mm_clflush, __rdtscp (same as asm)

// Reduced REPEAT for quicker testing, especially for large memory sizes
#ifndef REPEAT
#define REPEAT 10000
#endif
#define WARMUP 10
#define CACHELINE 64
#define PAGE 4096

static inline void clflush_range(void *p, size_t len) {
    uintptr_t addr = (uintptr_t)p;
    uintptr_t end  = addr + len;
    for (; addr < end; addr += CACHELINE) {
        _mm_clflush((void*)addr);
    }
    _mm_mfence();  // ensure flush completion
}

static inline uint64_t tsc_begin(void) {
    unsigned int aux;
    _mm_lfence();                // serialize before reading TSC
    return __rdtscp(&aux);       // read tsc with ordering
}

static inline uint64_t tsc_end(void) {
    unsigned int aux;
    uint64_t t = __rdtscp(&aux); // read tsc with ordering
    _mm_lfence();         // serialize after reading TSC
    return t;
}

static inline void prefault_touch(char *buf, size_t bytes) {
    // Page pre-touch to avoid page faults/zeroing during timing
    for (size_t i = 0; i < bytes; i += PAGE) buf[i] = 1;
    if (bytes > 0) buf[bytes-1] ^= 0; // touch last byte to prevent OOB
}

static inline void memtest(size_t bytes, FILE *out) {
    // 64B aligned allocation (to avoid false sharing across cache lines)
    char *src, *dst;
    if (posix_memalign((void**)&src, CACHELINE, bytes) ||
        posix_memalign((void**)&dst, CACHELINE, bytes)) {
        perror("posix_memalign"); exit(1);
    }

    // Initialize & pre-touch pages
    memset(src, 0xA5, bytes);
    memset(dst, 0,    bytes);
    prefault_touch(src, bytes);
    prefault_touch(dst, bytes);

    // Warmup: establish i-cache path/page tables/TLBs etc., not timed
    for (int r = 0; r < WARMUP; ++r) {
        clflush_range(src, bytes);
        clflush_range(dst, bytes);
        (void)memcpy(dst, src, bytes);
    }

    // Start actual measurement
    for (int r = 0; r < REPEAT; ++r) {
        // flush all cache lines that will be touched this iteration
        clflush_range(src, bytes);
        clflush_range(dst, bytes);

        uint64_t t0 = tsc_begin();
        memcpy(dst, src, bytes);
        uint64_t t1 = tsc_end();

        // prevent compiler optimizing away memcpy
        asm volatile("" :: "r"(dst[0]) : "memory");

        // record results to CSV
        fprintf(out, "%zu,%" PRIu64 "\n", bytes, (t1 - t0));
    }

    free(src);
    free(dst);
}

int main(void) {
    FILE *out = fopen("results.csv", "w");
    if (!out) { perror("fopen"); return 1; }
    fprintf(out, "Size(Bytes),Time(Ticks)\n");

    // iterate over sizes from 2^6 to 2^21
    const int exps[] = {6,7,8,9,10,11,12,13,14,15,16,20,21};
    for (size_t i = 0; i < sizeof(exps)/sizeof(exps[0]); ++i) {
        size_t bytes = (size_t)1 << exps[i];
        memtest(bytes, out);
    }
    fclose(out);
    return 0;
}
