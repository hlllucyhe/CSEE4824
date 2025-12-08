#define _POSIX_C_SOURCE 200112L  // for clock_gettime, posix_memalign

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/resource.h>
#include <omp.h>
#include <string.h>
#include <stdint.h>

// --------- CONFIG ----------

// minimal run length (number of elements) to enable OpenMP parallelism
#define OMP_THRESHOLD   (1 << 10)      // can tune this; e.g., 1024 elements

// cache-related parameters
#define CACHE_LINE_SIZE 64
#define TILE_ELEMS      2048           // number of elements loaded into each tile buffer

#define MIN(a,b) (( (a) < (b) ) ? (a) : (b))

// --------- Aligned allocation (cache line aligned) ----------
static int* allocate_aligned(size_t count) {
    void* ptr = NULL;
    size_t bytes = count * sizeof(int);
    if (posix_memalign(&ptr, CACHE_LINE_SIZE, bytes) != 0) {
        fprintf(stderr, "posix_memalign failed\n");
        exit(EXIT_FAILURE);
    }
    return (int*)ptr;
}

// --------- Peak RSS ----------
void print_memory_usage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("Max RSS: %ld kB (%.2f MB)\n",
           usage.ru_maxrss,
           usage.ru_maxrss / 1024.0);
}

// --------- Time ----------
double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

// --------- Load data (binary file of int32) ----------
int *load_data(const char *filename, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0 || file_size % sizeof(int) != 0) {
        fprintf(stderr, "Invalid file size\n");
        exit(EXIT_FAILURE);
    }

    *size = (size_t)(file_size / sizeof(int));

    int *arr = allocate_aligned(*size);
    if (!arr) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    size_t read_count = fread(arr, sizeof(int), *size, file);
    if (read_count != *size) {
        fprintf(stderr, "Failed to read entire data\n");
        exit(EXIT_FAILURE);
    }

    fclose(file);
    return arr;
}

// --------- Verify sorted ----------
int verify_sorted(const int *arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (arr[i] < arr[i - 1]) return 0;
    }
    return 1;
}

// --------- Tile-based, DRAM-friendly merge of two runs ----------
// src[left..mid] and src[mid+1..right] -> dst[left..right]
static void merge_runs_tiled(const int* src, int* dst, int left, int mid, int right) {
    // tile buffers on stack: thread-local, no sharing or race conditions
    int bufL[TILE_ELEMS];
    int bufR[TILE_ELEMS];

    int i_src = left;      // current position in left run
    int j_src = mid + 1;   // current position in right run
    int lenL = 0;          // number of valid elements in left tile
    int lenR = 0;          // number of valid elements in right tile
    int i = 0;             // pointer in left tile
    int j = 0;             // pointer in right tile

    int k = left;          // write position in dst

    // refill macros (local to this function)
#define REFILL_L() do {                                                \
        if (i_src > mid) {                                             \
            lenL = 0;                                                  \
        } else {                                                       \
            lenL = mid + 1 - i_src;                                    \
            if (lenL > TILE_ELEMS) lenL = TILE_ELEMS;                  \
            memcpy(bufL, src + i_src, lenL * sizeof(int));             \
            i_src += lenL;                                             \
            i = 0;                                                     \
        }                                                              \
    } while (0)

#define REFILL_R() do {                                                \
        if (j_src > right) {                                           \
            lenR = 0;                                                  \
        } else {                                                       \
            lenR = right + 1 - j_src;                                  \
            if (lenR > TILE_ELEMS) lenR = TILE_ELEMS;                  \
            memcpy(bufR, src + j_src, lenR * sizeof(int));             \
            j_src += lenR;                                             \
            j = 0;                                                     \
        }                                                              \
    } while (0)

    // initial tile fill
    REFILL_L();
    REFILL_R();

    // merge while both tiles have data
    while (lenL > 0 && lenR > 0) {
        while (i < lenL && j < lenR) {
            if (bufL[i] <= bufR[j]) {
                dst[k++] = bufL[i++];
                if (i == lenL) {        // left tile exhausted
                    REFILL_L();
                    if (lenL == 0) break;
                }
            } else {
                dst[k++] = bufR[j++];
                if (j == lenR) {        // right tile exhausted
                    REFILL_R();
                    if (lenR == 0) break;
                }
            }
        }
    }

    // flush remaining left tiles (if any)
    while (lenL > 0) {
        while (i < lenL) {
            dst[k++] = bufL[i++];
        }
        REFILL_L();
    }

    // flush remaining right tiles (if any)
    while (lenR > 0) {
        while (j < lenR) {
            dst[k++] = bufR[j++];
        }
        REFILL_R();
    }

#undef REFILL_L
#undef REFILL_R
}

// --------- Bottom-up mergesort + OpenMP + tiled merge ----------
void mergeSort_seq_dram_parallel(int *arr, size_t n) {
    // allocate temporary buffer, aligned for cache efficiency
    int *tmp = allocate_aligned(n);
    if (!tmp) {
        perror("malloc tmp failed");
        exit(EXIT_FAILURE);
    }

    int *src = arr;
    int *dst = tmp;

    for (size_t width = 1; width < n; width *= 2) {

        // parallel if run size large enough; otherwise use serial loop
        if (2 * width >= OMP_THRESHOLD) {

#pragma omp parallel for schedule(static)
            for (size_t left = 0; left < n; left += 2 * width) 
            {
                size_t mid   = left + width - 1;
                size_t right = left + 2 * width - 1;

                if (mid >= n)   mid = n - 1;
                if (right >= n) right = n - 1;

                if (mid < right) {
                    // both runs exist → use tiled merge
                    merge_runs_tiled(src, dst, (int)left, (int)mid, (int)right);
                } else {
                    // only one run left → copy directly
                    for (size_t i = left; i <= right; i++)
                        dst[i] = src[i];
                }
            }

        } else {

            // serial version for small run sizes
            for (size_t left = 0; left < n; left += 2 * width) 
            {
                size_t mid   = left + width - 1;
                size_t right = left + 2 * width - 1;

                if (mid >= n)   mid = n - 1;
                if (right >= n) right = n - 1;

                if (mid < right) {
                    merge_runs_tiled(src, dst, (int)left, (int)mid, (int)right);
                } else {
                    for (size_t i = left; i <= right; i++)
                        dst[i] = src[i];
                }
            }
        }

        // swap src / dst for next iteration
        int *tmp_ptr = src;
        src = dst;
        dst = tmp_ptr;
    }

    // ensure final result is placed in arr[]
    if (src != arr) {
#pragma omp parallel for schedule(static)
        for (size_t i = 0; i < n; i++)
            arr[i] = src[i];
    }

    free(tmp);
}

// ---------- main ----------
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    size_t n;
    printf("Loading data from %s...\n", argv[1]);
    int *arr = load_data(argv[1], &n);
    printf("Loaded %zu integers\n", n);

    double start = omp_get_wtime();

#pragma omp parallel
    {
#pragma omp single
        mergeSort_seq_dram_parallel(arr, n);
    }

    double end = omp_get_wtime();
    double duration = end - start;

    printf("Time: %.6f sec\n", duration);
    print_memory_usage();

    // throughput metrics
    double gb_sorted = (n * sizeof(int)) / (1024.0 * 1024.0 * 1024.0);
    printf("Sorted data: %.4f GB\n", gb_sorted);
    printf("Throughput: %.4f GB/s\n", gb_sorted / duration);

    // validation
    printf("Verification: %s\n",
           verify_sorted(arr, n) ? "Sorted" : "NOT sorted");

    free(arr);
    return 0;
}