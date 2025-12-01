// cache_optimized.c - Pengpeng's cache optimization implementation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

// Cache line size (typically 64 bytes on modern CPUs)
#define CACHE_LINE_SIZE 64

// L1/L2 cache blocking parameters (tune these based on your CPU)
// L1 cache is typically 32KB-64KB per core
// L2 cache is typically 256KB-1MB per core
#define L1_BLOCK_SIZE 8192    // ~8KB blocks for L1 cache
#define L2_BLOCK_SIZE 65536   // ~64KB blocks for L2 cache

// Alignment macro - ensures data is aligned to cache line boundaries
#define ALIGN_TO_CACHELINE(x) (((x) + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1))

// Allocate cache-line aligned memory
int* allocate_aligned(size_t count) {
    size_t size = count * sizeof(int);
    size_t aligned_size = ALIGN_TO_CACHELINE(size);

    void* ptr = NULL;
    // Use posix_memalign for cache line alignment
    if (posix_memalign(&ptr, CACHE_LINE_SIZE, aligned_size) != 0) {
        fprintf(stderr, "Failed to allocate aligned memory\n");
        exit(EXIT_FAILURE);
    }
    return (int*)ptr;
}

// Cache-blocked merge function
// This merges two sorted subarrays while optimizing for cache locality
void merge_cache_blocked(int* arr, int left, int mid, int right, int* temp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    // Use pre-allocated aligned temporary buffer
    int* L = temp;
    int* R = temp + ALIGN_TO_CACHELINE(n1 * sizeof(int)) / sizeof(int);

    // Copy data in cache-friendly blocks
    // This improves hardware prefetching
    for (int i = 0; i < n1; i++)
        L[i] = arr[left + i];
    for (int j = 0; j < n2; j++)
        R[j] = arr[mid + 1 + j];

    // Merge with cache blocking
    int i = 0, j = 0, k = left;

    // Process in blocks to stay in L1 cache
    while (i < n1 && j < n2) {
        // Determine block size based on remaining elements
        int i_block_end = (i + L1_BLOCK_SIZE < n1) ? i + L1_BLOCK_SIZE : n1;
        int j_block_end = (j + L1_BLOCK_SIZE < n2) ? j + L1_BLOCK_SIZE : n2;

        // Process current block
        while (i < i_block_end && j < j_block_end) {
            if (L[i] <= R[j]) {
                arr[k++] = L[i++];
            }
            else {
                arr[k++] = R[j++];
            }
        }
    }

    // Copy remaining elements
    while (i < n1) {
        arr[k++] = L[i++];
    }
    while (j < n2) {
        arr[k++] = R[j++];
    }
}

// Cache-optimized merge sort
void mergeSort_cache_optimized(int* arr, int left, int right, int* temp) {
    if (left < right) {
        int mid = left + (right - left) / 2;

        mergeSort_cache_optimized(arr, left, mid, temp);
        mergeSort_cache_optimized(arr, mid + 1, right, temp);

        merge_cache_blocked(arr, left, mid, right, temp);
    }
}

// Wrapper function that handles aligned memory allocation
void mergeSort_wrapper(int* arr, int n) {
    // Allocate cache-aligned temporary buffer
    // Size needs to accommodate the largest merge operation
    int* temp = allocate_aligned(n);

    mergeSort_cache_optimized(arr, 0, n - 1, temp);

    free(temp);
}

// Load data from binary file
int* load_data(const char* filename, size_t* size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *size = file_size / sizeof(int);

    // Allocate cache-aligned memory for input data
    int* arr = allocate_aligned(*size);

    size_t read_count = fread(arr, sizeof(int), *size, file);
    if (read_count != *size) {
        fprintf(stderr, "Failed to read complete data\n");
        exit(EXIT_FAILURE);
    }

    fclose(file);
    return arr;
}

// Verify if array is sorted
int verify_sorted(int* arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (arr[i] < arr[i - 1]) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* filename = argv[1];
    size_t size;

    printf("Loading data from %s...\n", filename);
    int* arr = load_data(filename, &size);
    printf("Loaded %zu integers\n", size);

    printf("Starting cache-optimized merge sort...\n");
    clock_t start = clock();

    mergeSort_wrapper(arr, size);

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Sorting completed in %.4f seconds\n", time_spent);

    // Verify correctness
    if (verify_sorted(arr, size)) {
        printf("Verification: Array is correctly sorted!\n");
    }
    else {
        printf("Verification: ERROR - Array is NOT sorted!\n");
    }

    // Calculate throughput
    double gb_sorted = (size * sizeof(int)) / (1024.0 * 1024.0 * 1024.0);
    double throughput = gb_sorted / time_spent;
    printf("Throughput: %.4f GB/s\n", throughput);

    free(arr);
    return EXIT_SUCCESS;
}