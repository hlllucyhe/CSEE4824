// cache_optimized.c - ÐÞÕý°æ
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

// Cache line size (typically 64 bytes on modern CPUs)
#define CACHE_LINE_SIZE 64

// L1/L2 cache blocking parameters
#define L1_BLOCK_SIZE 8192    // ~8KB blocks for L1 cache

// Alignment macro
#define ALIGN_TO_CACHELINE(x) (((x) + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1))

// Allocate cache-line aligned memory
int* allocate_aligned(size_t count) {
    size_t size = count * sizeof(int);
    size_t aligned_size = ALIGN_TO_CACHELINE(size);

    void* ptr = NULL;
    if (posix_memalign(&ptr, CACHE_LINE_SIZE, aligned_size) != 0) {
        fprintf(stderr, "Failed to allocate aligned memory\n");
        exit(EXIT_FAILURE);
    }
    return (int*)ptr;
}

// Cache-blocked merge function
void merge_cache_blocked(int* arr, int left, int mid, int right, int* temp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    // Use pre-allocated temporary buffer
    int* L = temp;
    int* R = temp + n1;  // ÐÞÕý£º¼ò»¯Æ«ÒÆ¼ÆËã

    // Copy data to temporary buffers
    memcpy(L, &arr[left], n1 * sizeof(int));
    memcpy(R, &arr[mid + 1], n2 * sizeof(int));

    // Merge with cache blocking
    int i = 0, j = 0, k = left;

    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k++] = L[i++];
        }
        else {
            arr[k++] = R[j++];
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

// Wrapper function
void mergeSort_wrapper(int* arr, int n) {
    // Allocate temporary buffer (needs to hold n elements)
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

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *size = file_size / sizeof(int);

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
    printf("Data size: %.4f GB\n", gb_sorted);
    printf("Throughput: %.4f GB/s\n", throughput);

    free(arr);
    return EXIT_SUCCESS;
}