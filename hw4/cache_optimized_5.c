// aligned_merge_sort.c
// Merge sort with memory alignment optimization only.
// Compile: gcc -O3 -march=native aligned_merge_sort.c -o aligned_merge
// Run:     ./aligned_merge input.bin

#define _POSIX_C_SOURCE 200112L  // for posix_memalign

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define CACHE_LINE_SIZE 64

//---------------------------------------------
// Allocate 64-byte aligned int array
//---------------------------------------------
static int* allocate_aligned(size_t count) {
    void* ptr = NULL;
    size_t size = count * sizeof(int);

    if (posix_memalign(&ptr, CACHE_LINE_SIZE, size) != 0) {
        fprintf(stderr, "posix_memalign failed\n");
        exit(EXIT_FAILURE);
    }
    return (int*)ptr;
}

//---------------------------------------------
// Standard merge
//---------------------------------------------
static void merge(int* arr, int left, int mid, int right, int* temp) {
    int i = left;
    int j = mid + 1;
    int k = left;

    // Merge into temp
    while (i <= mid && j <= right) {
        if (arr[i] <= arr[j]) temp[k++] = arr[i++];
        else                 temp[k++] = arr[j++];
    }

    while (i <= mid)  temp[k++] = arr[i++];
    while (j <= right) temp[k++] = arr[j++];

    // Copy back
    memcpy(&arr[left], &temp[left], (right - left + 1) * sizeof(int));
}

//---------------------------------------------
// Recursive merge sort
//---------------------------------------------
static void merge_sort_rec(int* arr, int left, int right, int* temp) {
    if (left >= right) return;

    int mid = left + (right - left) / 2;
    merge_sort_rec(arr, left, mid, temp);
    merge_sort_rec(arr, mid + 1, right, temp);
    merge(arr, left, mid, right, temp);
}

//---------------------------------------------
// Entry
//---------------------------------------------
static void merge_sort_aligned(int* arr, size_t n) {
    int* temp = allocate_aligned(n);
    merge_sort_rec(arr, 0, (int)n - 1, temp);
    free(temp);
}

//---------------------------------------------
// Load binary int32 dataset
//---------------------------------------------
static int* load_data(const char* filename, size_t* size_out) {
    FILE* f = fopen(filename, "rb");
    if (!f) { perror("fopen"); exit(EXIT_FAILURE); }

    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (bytes <= 0 || bytes % sizeof(int) != 0) {
        fprintf(stderr, "Invalid file\n");
        exit(EXIT_FAILURE);
    }

    size_t n = bytes / sizeof(int);
    int* arr = allocate_aligned(n);

    if (fread(arr, sizeof(int), n, f) != n) {
        fprintf(stderr, "Read failed\n");
        exit(EXIT_FAILURE);
    }
    fclose(f);
    *size_out = n;
    return arr;
}

//---------------------------------------------
// Verify sorted
//---------------------------------------------
static int verify_sorted(const int* arr, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (arr[i] < arr[i - 1]) return 0;
    return 1;
}

//---------------------------------------------
// Main
//---------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_binary_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    size_t n = 0;
    printf("Loading data from %s...\n", argv[1]);
    int* arr = load_data(argv[1], &n);
    printf("Loaded %zu integers\n", n);

    clock_t start = clock();
    merge_sort_aligned(arr, n);
    clock_t end = clock();

    double sec = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Sorting completed in %.4f seconds\n", sec);

    if (verify_sorted(arr, n))
        printf("Verification: Array is correctly sorted!\n");
    else
        printf("Verification: NOT sorted!\n");

    double gb = (n * sizeof(int)) / (1024.0 * 1024.0 * 1024.0);
    double thr = gb / sec;
    printf("Data size: %.4f GB, Throughput: %.4f GB/s\n", gb, thr);

    free(arr);
    return 0;
}
