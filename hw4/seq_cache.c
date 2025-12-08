// sequential_dram_cache_merged_omp.c
// DRAM-friendly + Cache-tiled MergeSort (sequential)
// 使用 OpenMP 的 omp_get_wtime() 计时

#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/resource.h>
#include <omp.h>

#define CACHE_LINE_SIZE     64
#define TILE_ELEMS          2048
#define INSERTION_THRESHOLD 32
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))

// ------------------------------------------------------------
// Memory usage
// ------------------------------------------------------------
void print_memory_usage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("Max RSS: %ld kB (%.2f MB)\n",
           usage.ru_maxrss,
           usage.ru_maxrss / 1024.0);
}

// ------------------------------------------------------------
// Aligned allocation
// ------------------------------------------------------------
static int* allocate_aligned(size_t count) {
    void* ptr = NULL;
    size_t bytes = count * sizeof(int);

    if (posix_memalign(&ptr, CACHE_LINE_SIZE, bytes) != 0) {
        fprintf(stderr, "posix_memalign failed\n");
        exit(EXIT_FAILURE);
    }
    return (int*)ptr;
}

// ------------------------------------------------------------
// Insertion Sort (for small segments, better cache behavior)
// ------------------------------------------------------------
static void insertion_sort(int* arr, int left, int right) {
    for (int i = left + 1; i <= right; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= left && arr[j] > key) {
            arr[j+1] = arr[j];
            j--;
        }
        arr[j+1] = key;
    }
}

// ------------------------------------------------------------
// Cache-tiled merge
// ------------------------------------------------------------
static void merge_tiled(int* arr, int left, int mid, int right, int* temp) {

    int n1 = mid - left + 1;
    int n2 = right - mid;

    int* Lsrc = temp;
    int* Rsrc = temp + n1;

    memcpy(Lsrc, &arr[left],   n1 * sizeof(int));
    memcpy(Rsrc, &arr[mid+1],  n2 * sizeof(int));

    int bufL[TILE_ELEMS];
    int bufR[TILE_ELEMS];

    int i_src = 0, j_src = 0;
    int lenL = 0, lenR = 0;
    int i = 0, j = 0;

    int k = left;

#define REFILL_L() do {                                        \
    if (i_src >= n1) { lenL = 0; }                              \
    else {                                                      \
        lenL = MIN(TILE_ELEMS, n1 - i_src);                    \
        memcpy(bufL, Lsrc + i_src, lenL * sizeof(int));       \
        i_src += lenL;                                         \
        i = 0;                                                 \
    } } while(0)

#define REFILL_R() do {                                        \
    if (j_src >= n2) { lenR = 0; }                              \
    else {                                                      \
        lenR = MIN(TILE_ELEMS, n2 - j_src);                    \
        memcpy(bufR, Rsrc + j_src, lenR * sizeof(int));       \
        j_src += lenR;                                         \
        j = 0;                                                 \
    } } while(0)

    REFILL_L();
    REFILL_R();

    while (lenL > 0 && lenR > 0) {
        while (i < lenL && j < lenR) {
            if (bufL[i] <= bufR[j]) {
                arr[k++] = bufL[i++];
                if (i == lenL) { REFILL_L(); if (lenL == 0) break; }
            } else {
                arr[k++] = bufR[j++];
                if (j == lenR) { REFILL_R(); if (lenR == 0) break; }
            }
        }
    }

    while (lenL > 0) {
        while (i < lenL) arr[k++] = bufL[i++];
        REFILL_L();
    }
    while (lenR > 0) {
        while (j < lenR) arr[k++] = bufR[j++];
        REFILL_R();
    }

    if (i_src < n1) {
        memcpy(&arr[k], &Lsrc[i_src], (n1 - i_src) * sizeof(int));
        k += (n1 - i_src);
    }
    if (j_src < n2) {
        memcpy(&arr[k], &Rsrc[j_src], (n2 - j_src) * sizeof(int));
        k += (n2 - j_src);
    }

#undef REFILL_L
#undef REFILL_R
}

// ------------------------------------------------------------
// Recursive merge sort using tiled merge
// ------------------------------------------------------------
static void mergeSort_tiled_rec(int* arr, int left, int right, int* temp) {

    if (left >= right) return;

    int n = right - left + 1;

    if (n <= INSERTION_THRESHOLD) {
        insertion_sort(arr, left, right);
        return;
    }

    int mid = left + (right - left) / 2;
    mergeSort_tiled_rec(arr, left, mid, temp);
    mergeSort_tiled_rec(arr, mid + 1, right, temp);

    merge_tiled(arr, left, mid, right, temp);
}

// ------------------------------------------------------------
// Public API: mergeSort_seq()
// ------------------------------------------------------------
void mergeSort_seq(int* arr, size_t n) {
    if (n <= 1) return;

    int* temp = allocate_aligned(n);
    mergeSort_tiled_rec(arr, 0, (int)n - 1, temp);
    free(temp);
}

// ------------------------------------------------------------
// Load binary data
// ------------------------------------------------------------
int* load_data(const char* filename, size_t* size_out) {

    FILE* f = fopen(filename, "rb");
    if (!f) { perror("open"); exit(1); }

    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (bytes < 0 || bytes % sizeof(int) != 0) {
        fprintf(stderr, "Invalid file\n");
        exit(1);
    }

    size_t n = bytes / sizeof(int);
    int* arr = allocate_aligned(n);

    fread(arr, sizeof(int), n, f);
    fclose(f);

    *size_out = n;
    return arr;
}

// ------------------------------------------------------------
// Verify sorted
// ------------------------------------------------------------
int verify_sorted(const int* arr, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (arr[i] < arr[i-1]) return 0;
    return 1;
}

// ------------------------------------------------------------
// MAIN — uses omp_get_wtime()
// ------------------------------------------------------------
int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Usage: %s <data_file>\n", argv[0]);
        return 1;
    }

    size_t n = 0;
    printf("Loading data...\n");
    int* arr = load_data(argv[1], &n);
    printf("Loaded %zu integers.\n", n);

    double start = omp_get_wtime();
    mergeSort_seq(arr, n);
    double end = omp_get_wtime();

    double secs = end - start;
    printf("Sequential DRAM + Cache-Tiled Sort Time: %.6f s\n", secs);
    print_memory_usage();

    printf("Verification: %s\n", verify_sorted(arr, n) ? "sorted" : "NOT sorted");

    double gb = (n * sizeof(int)) / (1024.0 * 1024.0 * 1024.0);
    printf("Throughput: %.3f GB/s\n", gb / secs);

    free(arr);
    return 0;
}