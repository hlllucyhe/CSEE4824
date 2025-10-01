#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <x86intrin.h>  

#define REPEAT 10
#define BUFFER_SIZE (64 * 1024 * 1024) // 64MB to ensure we're accessing DRAM
#define STRIDE_SIZE (8 * 1024)         // 8KB stride, typical DRAM row size

#define CACHELINE 64

static inline void clflush_range(void *p, size_t len) {
    uintptr_t addr = (uintptr_t)p;
    uintptr_t end  = addr + len;

    for (; addr < end; addr += CACHELINE) {
        _mm_clflush((void*)addr);
    }
    _mm_mfence();  
}

inline uint64_t rdtsc() {
    unsigned long a, d;
    asm volatile ("rdtsc" : "=a" (a), "=d" (d));
    return a | ((uint64_t)d << 32);
}

long int rep;

void test_open_vs_closed_row() {
    // Allocate large buffers to ensure we're working in DRAM
    char* buffer1 = (char*) mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, 
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    char* buffer2 = (char*) mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, 
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (buffer1 == MAP_FAILED || buffer2 == MAP_FAILED) {
        printf("Memory allocation failed\n");
        return;
    }
    
    // Initialize buffers
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer1[i] = (char)(i % 256);
        buffer2[i] = 0;
    }
    
    uint64_t start, end;
    uint64_t clock1 = 0, clock2 = 0, clock3 = 0; 
    
    volatile char *addr1 = buffer1;
    volatile char *addr2 = buffer1 + STRIDE_SIZE; // Different row
    
    for (rep = 0; rep < REPEAT; rep++) {
        // Flush both addresses
        clflush_range(buffer1, BUFFER_SIZE);
        clflush_range(buffer2, BUFFER_SIZE);

        // First access to row 1
        start = rdtsc();
        *addr1 = 'C';
        end = rdtsc();
        clock1 = (end - start);
        printf("First access to row1: %llu ticks\n", clock1);

        
        clflush_range(buffer1, BUFFER_SIZE);
        clflush_range(buffer2, BUFFER_SIZE);
        // Access to different row2
        start = rdtsc();
        *addr2 = 'D';
        end = rdtsc();
        clock2 = (end - start);
        printf("First access to row2: %llu ticks\n", clock2);


        clflush_range(buffer1, BUFFER_SIZE);
        clflush_range(buffer2, BUFFER_SIZE);
        // Access to same row2 (again)
        start = rdtsc();
        *addr2 = 'D';
        end = rdtsc();
        clock3 = (end - start);
        printf("Second access to row2: %llu ticks\n", clock3);


    }

    /*
    // 输出平均值
    printf("===== Row Buffer Policy Test =====\n");
    printf("Avg access row1: %" PRIu64 " cycles\n", clock1 / REPEAT);
    printf("Avg access row2 (first): %" PRIu64 " cycles\n", clock2 / REPEAT);
    printf("Avg access row2 (second): %" PRIu64 " cycles\n", clock3 / REPEAT);*/

   // Clean up
    munmap(buffer1, BUFFER_SIZE);
    munmap(buffer2, BUFFER_SIZE);

}



int main(int ac, char **av) {
    printf("Testing DRAM Row Buffer Policy\n");
    printf("==============================\n");
    
    test_open_vs_closed_row();
    return 0;
}