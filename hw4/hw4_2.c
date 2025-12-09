#define _POSIX_C_SOURCE 200112L  // for posix_memalign

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <omp.h>
#include <sys/resource.h>

#define THRESHOLD            50000      // threshold for switching to serial sort
#define CACHE_LINE_SIZE      64
#define TILE_ELEMS           2048       // tile size for cache blocking
#define INSERTION_THRESHOLD  32         // use insertion sort for small segments
#define MIN(a,b)             (((a) < (b)) ? (a) : (b))

// ---------- Peak RSS ----------
void print_memory_usage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("Max RSS: %ld kB (%.2f MB)\n",
           usage.ru_maxrss,
           usage.ru_maxrss / 1024.0);
}

// ---------- Aligned memory allocator ----------
static int* allocate_aligned(size_t count) {
    if (count == 0) return NULL;
    void* ptr = NULL;
    size_t size_in_bytes = count * sizeof(int);
    if (posix_memalign(&ptr, CACHE_LINE_SIZE, size_in_bytes) != 0) {
        fprintf(stderr, "posix_memalign failed\n");
        exit(EXIT_FAILURE);
    }
    return (int*)ptr;
}

// ---------- Insertion sort for small segments ----------
static void insertion_sort(int* arr, size_t left, size_t right) {
    for (size_t i = left + 1; i <= right; i++) {
        int key = arr[i];
        size_t j = i;
        while (j > left && arr[j - 1] > key) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = key;
    }
}

// ---------- Cache-tiled merge ----------
// temp_full is a global scratch buffer for the entire array.
// Each merge uses temp_full[left..right], which does not conflict with other merges.
static void merge_tiled(int* arr, size_t left, size_t mid, size_t right, int* temp_full) {
    size_t n1 = mid - left + 1;
    size_t n2 = right - mid;
    int* base = temp_full + left;
    int* Lsrc = base;          // temp[left .. left+n1-1]
    int* Rsrc = base + n1;     // temp[left+n1 .. left+n1+n2-1]

    // Copy both runs contiguously into the temp buffer
    memcpy(Lsrc, &arr[left],   n1 * sizeof(int));
    memcpy(Rsrc, &arr[mid+1],  n2 * sizeof(int));

    // Tile buffers to improve cache locality
    int bufL[TILE_ELEMS];
    int bufR[TILE_ELEMS];

    size_t i_src = 0, j_src = 0;
    size_t lenL = 0, lenR = 0;
    size_t i = 0, j = 0;

    size_t k = left;  // write position into arr

#define REFILL_L() do {                                                \
    if (i_src >= n1) {                                                 \
        lenL = 0;                                                      \
    } else {                                                           \
        lenL = MIN(TILE_ELEMS, n1 - i_src);                            \
        memcpy(bufL, Lsrc + i_src, lenL * sizeof(int));                \
        i_src += lenL;                                                 \
        i = 0;                                                         \
    } } while (0)

#define REFILL_R() do {                                                \
    if (j_src >= n2) {                                                 \
        lenR = 0;                                                      \
    } else {                                                           \
        lenR = MIN(TILE_ELEMS, n2 - j_src);                            \
        memcpy(bufR, Rsrc + j_src, lenR * sizeof(int));                \
        j_src += lenR;                                                 \
        j = 0;                                                         \
    } } while (0)

    REFILL_L();
    REFILL_R();

    // Main merge loop using tile buffers
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

    // Drain remaining elements from L
    while (lenL > 0) {
        while (i < lenL) arr[k++] = bufL[i++];
        REFILL_L();
    }

    // Drain remaining elements from R
    while (lenR > 0) {
        while (j < lenR) arr[k++] = bufR[j++];
        REFILL_R();
    }

    // Copy any trailing elements that did not get tiled
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

// ---------- Serial (cache-tiled) merge sort recursion ----------
static void mergeSort_serial_cache(int* arr, size_t left, size_t right, int* temp) {
    if (left >= right) return;

    size_t n = right - left + 1;
    if (n <= INSERTION_THRESHOLD) {
        insertion_sort(arr, left, right);
        return;
    }

    size_t mid = left + (right - left) / 2;
    mergeSort_serial_cache(arr, left,     mid, temp);
    mergeSort_serial_cache(arr, mid + 1,  right, temp);

    merge_tiled(arr, left, mid, right, temp);
}

// ---------- Parallel + cache-tiled merge sort (same behavior as original parallel.c) ----------
static void mergeSort_parallel_cache(int* arr, size_t left, size_t right, int* temp) {
    if (left >= right) return;

    size_t n = right - left + 1;
    if (n <= THRESHOLD) {
        // Use serial cache-optimized version for small segments
        mergeSort_serial_cache(arr, left, right, temp);
        return;
    }

    size_t mid = left + (right - left) / 2;

    // Parallel tasks for recursive calls (same pattern as original parallel.c)
#pragma omp task shared(arr, temp)
    mergeSort_parallel_cache(arr, left, mid, temp);

#pragma omp task shared(arr, temp)
    mergeSort_parallel_cache(arr, mid + 1, right, temp);

#pragma omp taskwait
    merge_tiled(arr, left, mid, right, temp);
}

// ---------- File loading ----------
int* load_data(const char* filename, size_t* size_out) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0 || file_size % (long)sizeof(int) != 0) {
        fprintf(stderr, "Invalid file size\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    size_t n = (size_t)(file_size / (long)sizeof(int));
    int* arr = allocate_aligned(n);

    size_t read_count = fread(arr, sizeof(int), n, file);
    if (read_count != n) {
        fprintf(stderr, "Failed to read complete data\n");
        fclose(file);
        free(arr);
        exit(EXIT_FAILURE);
    }

    fclose(file);
    *size_out = n;
    return arr;
}

// ---------- Sortedness verification ----------
int verify_sorted(const int* arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (arr[i] < arr[i - 1]) return 0;
    }
    return 1;
}

// ---------- main ----------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_binary_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* filename = argv[1];
    size_t n = 0;

    printf("Loading data from %s...\n", filename);
    int* arr = load_data(filename, &n);
    printf("Loaded %zu integers\n", n);

    if (n == 0) {
        printf("Empty input, nothing to sort.\n");
        free(arr);
        return EXIT_SUCCESS;
    }

    // Allocate global temp buffer shared by all recursive tasks
    int* temp = allocate_aligned(n);

    printf("Starting parallel + cache-tiled merge sort...\n");
    double start = omp_get_wtime();

#pragma omp parallel
    {
#pragma omp single
        mergeSort_parallel_cache(arr, 0, n - 1, temp);
    }

    double end = omp_get_wtime();
    double time_spent = end - start;

    printf("Sorting completed in %.6f seconds\n", time_spent);
    print_memory_usage();

    if (verify_sorted(arr, n))
        printf("Verification: Array is correctly sorted!\n");
    else
        printf("Verification: ERROR - Array is NOT sorted!\n");

    double gb_sorted = (n * sizeof(int)) / (1024.0 * 1024.0 * 1024.0);
    double throughput = gb_sorted / time_spent;

    printf("Data size: %.4f GB\n", gb_sorted);
    printf("Throughput: %.4f GB/s\n", throughput);

    free(temp);
    free(arr);
    return EXIT_SUCCESS;
}
