#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define REPEAT 100
#define BUFFER_SIZE (64 * 1024 * 1024) // 64MB to ensure we're accessing DRAM
#define STRIDE_SIZE (8 * 1024) // 8KB stride, typical DRAM row size

inline void clflush(volatile void *p) {
    asm volatile ("clflush (%0)" :: "r"(p));
}

inline uint64_t rdtsc() {
    unsigned long a, d;
    asm volatile ("rdtsc" : "=a" (a), "=d" (d));
    return a | ((uint64_t)d << 32);
}

inline void memory(void *dst, void *src, size_t n) {
    memcpy(dst, src, n);
}

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
    uint64_t time_first_access, time_second_access;
    
    volatile char *addr1 = buffer1;
    volatile char *addr2 = buffer1 + STRIDE_SIZE; // Different row
    
    
    printf("\nTesting different row access:\n");
    
    // First access to row 1
    start = rdtsc();
    *addr1 = 'C';
    end = rdtsc();
    time_first_access = end - start;
    printf("First access to row 1: %llu ticks\n", time_first_access);
    
    // Flush both addresses
    clflush(addr1);
    clflush(addr2);
    
    // Access to different row2
    start = rdtsc();
    *addr2 = 'D';
    end = rdtsc();
    time_second_access = end - start;
    printf("First access to row2: %llu ticks\n", time_second_access);

    // Access to same row2
    start = rdtsc();
    *addr2 = 'C';
    end = rdtsc();
    time_second_access = end - start;
    printf("Second access to row2: %llu ticks\n", time_second_access);

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

