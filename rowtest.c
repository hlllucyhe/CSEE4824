#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>   // for __rdtscp, _mm_clflush

#define REPEAT 100
#define CACHELINE 64
#define ROW_SIZE (32*1024)   // row size = 32KB

inline void clflush(volatile void *p) {
    _mm_clflush(p);
}

inline void mfence() {
    _mm_mfence();
}

inline uint64_t rdtsc() {
    unsigned int aux;
    return __rdtscp(&aux);
}

// Function to access entire cache line to ensure it's brought into cache
void access_memory(volatile char *addr) {
    *addr = *addr;  // Simple read to trigger memory access
}

void rowtest() {
    char *buf;
    // Allocate 4 rows to ensure we have different rows
    if (posix_memalign((void**)&buf, ROW_SIZE, ROW_SIZE * 4)) {
        perror("posix_memalign");
        exit(1);
    }

    // Initialize memory to prevent optimization
    memset(buf, 0, ROW_SIZE * 4);

    // Addresses for testing
    volatile char *base_addr = buf;
    volatile char *same_row_addr = buf + CACHELINE;  // Same row, different cache line
    volatile char *diff_row_addr = buf + ROW_SIZE;   // Different row
    
    uint64_t sum_first_same = 0, sum_second_same = 0;
    uint64_t sum_first_diff = 0, sum_second_diff = 0;

    printf("Testing row buffer policy...\n");
    printf("Base address: %p\n", base_addr);
    printf("Same row address: %p\n", same_row_addr);
    printf("Different row address: %p\n", diff_row_addr);

    for (int r = 0; r < REPEAT; r++) {
        // ==== SAME ROW CASE ====
        // Flush all relevant addresses from cache
        clflush(base_addr);
        clflush(same_row_addr);
        mfence();
        
        // First access - should open the row
        uint64_t t0 = rdtsc();
        access_memory(base_addr);
        mfence();
        uint64_t t1 = rdtsc();
        sum_first_same += (t1 - t0);

        // Second access to same row - should be fast if row remains open
        t0 = rdtsc();
        access_memory(same_row_addr);
        mfence();
        t1 = rdtsc();
        sum_second_same += (t1 - t0);

        // ==== DIFFERENT ROW CASE ====
        // Flush all relevant addresses
        clflush(base_addr);
        clflush(diff_row_addr);
        mfence();
        
        // First access - opens first row
        t0 = rdtsc();
        access_memory(base_addr);
        mfence();
        t1 = rdtsc();
        sum_first_diff += (t1 - t0);

        // Second access to different row - may require row closure and new row opening
        t0 = rdtsc();
        access_memory(diff_row_addr);
        mfence();
        t1 = rdtsc();
        sum_second_diff += (t1 - t0);
    }

    printf("\n=== Results (averaged over %d runs) ===\n", REPEAT);
    printf("Same-row case:\n");
    printf("  First access:  %lu cycles\n", sum_first_same / REPEAT);
    printf("  Second access: %lu cycles\n", sum_second_same / REPEAT);
    printf("  Speedup: %.2fx\n", (double)sum_first_same / sum_second_same);
    
    printf("Different-row case:\n");
    printf("  First access:  %lu cycles\n", sum_first_diff / REPEAT);
    printf("  Second access: %lu cycles\n", sum_second_diff / REPEAT);
    printf("  Speedup: %.2fx\n", (double)sum_first_diff / sum_second_diff);
    
    // Determine policy
    printf("\n=== Row Buffer Policy Analysis ===\n");
    double same_row_speedup = (double)sum_first_same / sum_second_same;
    double diff_row_speedup = (double)sum_first_diff / sum_second_diff;
    
    if (same_row_speedup > 1.5 && diff_row_speedup < 1.2) {
        printf("CONCLUSION: OPEN-ROW policy detected\n");
        printf("  - Same-row accesses show significant speedup (row remains open)\n");
        printf("  - Different-row accesses show little speedup (row conflict)\n");
    } else if (same_row_speedup > 1.2 && diff_row_speedup > 1.2) {
        printf("CONCLUSION: CLOSED-ROW policy detected\n");
        printf("  - Both same-row and different-row accesses show speedup\n");
    } else {
        printf("CONCLUSION: Inconclusive - may need more runs or different parameters\n");
    }

    free(buf);
}

int main() {
    printf("===== DRAM Row Buffer Policy Test =====\n");
    rowtest();
    return 0;
}