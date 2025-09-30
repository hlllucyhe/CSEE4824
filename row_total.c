#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>   
#include <unistd.h>
#include <sys/mman.h>

#define REPEAT 500000
#define ROW_STRIDE (8*1024)   // row size = 8kB
#define ALIGN     (64*1024)   // avoid physical mapping jitter

static inline void clflush_line(volatile void *p) {
    _mm_clflush(p);
    _mm_mfence();
}

static inline uint64_t rdtsc_begin(void) {
    unsigned int eax, ebx, ecx, edx;
    // serialize before
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx),
                                  "=c"(ecx), "=d"(edx)
                               : "a"(0) : "memory");
    unsigned int aux;
    _mm_lfence();
    return __rdtscp(&aux);
}

static inline uint64_t rdtsc_end(void) {
    unsigned int aux;
    uint64_t t = __rdtscp(&aux);
    _mm_lfence();
    unsigned int eax, ebx, ecx, edx;
    // serialize after
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx),
                                  "=c"(ecx), "=d"(edx)
                               : "a"(0) : "memory");
    return t;
}

int main(void) {
    printf("===== Row Buffer Policy Test (serialized 2-access total) =====\n");

    
    mlockall(MCL_CURRENT | MCL_FUTURE);

    
    char *buf;
    if (posix_memalign((void**)&buf, ALIGN, ALIGN * 4)) {
        perror("posix_memalign");
        return 1;
    }
    memset(buf, 0, ALIGN * 4);

    volatile unsigned long sink = 0; // avoid improvement
    char *addr1      = buf;
    char *addr2_same = buf + 64;            // same ror
    char *addr2_diff = buf + ROW_STRIDE;    // different row

    // warm-up (TLB + page reside)
    sink += *addr1;
    sink += *addr2_same;
    sink += *addr2_diff;

    // ===== SAME-ROW CASE=====
    uint64_t sum_same_total = 0;
    for (int r = 0; r < REPEAT; r++) {
        clflush_line(addr1);
        clflush_line(addr2_same);

        // time start
        uint64_t t0 = rdtsc_begin();

        
        unsigned char v1 = *(volatile unsigned char*)addr1;
        
        _mm_lfence();


        const char *p2 = addr2_same + (v1 & 0);
        unsigned char v2 = *(volatile unsigned char*)p2;

        // time over
        uint64_t t1 = rdtsc_end();
        sum_same_total += (t1 - t0);

        sink += v1 + v2;
    }

    // ===== DIFF-ROW CASE =====
    uint64_t sum_diff_total = 0;
    for (int r = 0; r < REPEAT; r++) {
        clflush_line(addr1);
        clflush_line(addr2_diff);

        uint64_t t0 = rdtsc_begin();

        unsigned char v1 = *(volatile unsigned char*)addr1;
        _mm_lfence();

        const char *p2 = addr2_diff + (v1 & 0);
        unsigned char v2 = *(volatile unsigned char*)p2;

        uint64_t t1 = rdtsc_end();
        sum_diff_total += (t1 - t0);

        sink += v1 + v2;
    }

    printf("Average over %d runs (ROW_STRIDE=%d KB):\n", REPEAT, (int)(ROW_STRIDE/1024));
    printf("  Same-row total (2 accesses): %lu cycles\n", (unsigned long)(sum_same_total / REPEAT));
    printf("  Diff-row total (2 accesses): %lu cycles\n", (unsigned long)(sum_diff_total / REPEAT));

    if (sink == 0xdeadbeef) puts("");

    free(buf);
    return 0;
}