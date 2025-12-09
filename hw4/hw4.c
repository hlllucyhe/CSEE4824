#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <omp.h>

// --------- CONFIG ----------
#define OMP_THRESHOLD   (1 << 18)
#define CACHE_LINE_SIZE 64
#define TILE_ELEMS      2048

// --------- Utility ----------
static int32_t* allocate_aligned(size_t count) {
    void* ptr = NULL;
    if (posix_memalign(&ptr, CACHE_LINE_SIZE, count * sizeof(int32_t)) != 0) {
        fprintf(stderr, "posix_memalign failed\n");
        exit(EXIT_FAILURE);
    }
    return (int32_t*)ptr;
}

void print_memory_usage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("Max RSS: %.2f MB\n", usage.ru_maxrss / 1024.0);
}

int verify_sorted(const int32_t *arr, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (arr[i] < arr[i - 1]) return 0;
    return 1;
}

// --------- File Loader ----------
int32_t *load_data(const char *filename, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (!file) { perror("open"); exit(1); }

    fseek(file, 0, SEEK_END);
    long bytes = ftell(file);
    fseek(file, 0, SEEK_SET);

    *size = (size_t)(bytes / (long)sizeof(int32_t));

    int32_t *arr = allocate_aligned(*size);

    size_t read_count = fread(arr, sizeof(int32_t), *size, file);
    if (read_count != *size) {
        fprintf(stderr, "Failed to read full file\n");
        exit(1);
    }

    fclose(file);
    return arr;
}

// --------- Tiled Merge ----------
static void merge_runs_tiled(const int32_t* src, int32_t* dst,
                             size_t left, size_t mid, size_t right)
{
    int32_t bufL[TILE_ELEMS];
    int32_t bufR[TILE_ELEMS];

    size_t i_src = left;
    size_t j_src = mid + 1;

    size_t lenL = 0, lenR = 0;
    size_t i = 0, j = 0;
    size_t k = left;

#define REFILL_L() do { \
    if (i_src > mid) lenL = 0; \
    else { \
        lenL = mid + 1 - i_src; \
        if (lenL > TILE_ELEMS) lenL = TILE_ELEMS; \
        memcpy(bufL, src + i_src, lenL * sizeof(int32_t)); \
        i_src += lenL; \
        i = 0; \
    } \
} while(0)

#define REFILL_R() do { \
    if (j_src > right) lenR = 0; \
    else { \
        lenR = right + 1 - j_src; \
        if (lenR > TILE_ELEMS) lenR = TILE_ELEMS; \
        memcpy(bufR, src + j_src, lenR * sizeof(int32_t)); \
        j_src += lenR; \
        j = 0; \
    } \
} while(0)

    REFILL_L();
    REFILL_R();

    while (lenL > 0 && lenR > 0) {
        while (i < lenL && j < lenR) {
            if (bufL[i] <= bufR[j]) {
                dst[k++] = bufL[i++];
                if (i == lenL) { REFILL_L(); if (lenL == 0) break; }
            } else {
                dst[k++] = bufR[j++];
                if (j == lenR) { REFILL_R(); if (lenR == 0) break; }
            }
        }
    }

    while (lenL > 0) {
        while (i < lenL) dst[k++] = bufL[i++];
        REFILL_L();
    }

    while (lenR > 0) {
        while (j < lenR) dst[k++] = bufR[j++];
        REFILL_R();
    }

#undef REFILL_L
#undef REFILL_R
}

// --------- Bottom-up + Parallel + Tiled ----------
void mergeSort_seq_dram_parallel(int32_t *arr, size_t n) {
    int32_t *tmp = allocate_aligned(n);
    int32_t *src = arr;
    int32_t *dst = tmp;

    for (size_t width = 1; width < n; width *= 2) {
        size_t run_size = 2 * width;

        if (run_size <= OMP_THRESHOLD) {

#pragma omp parallel for schedule(static)
            for (size_t left = 0; left < n; left += run_size) {
                size_t mid   = left + width - 1;
                size_t right = left + run_size - 1;

                if (mid >= n)   mid = n - 1;
                if (right >= n) right = n - 1;

                if (mid < right)
                    merge_runs_tiled(src, dst, left, mid, right);
                else
                    for (size_t i = left; i <= right; i++)
                        dst[i] = src[i];
            }
        } else {

            for (size_t left = 0; left < n; left += run_size) {
                size_t mid   = left + width - 1;
                size_t right = left + run_size - 1;

                if (mid >= n)   mid = n - 1;
                if (right >= n) right = n - 1;

                if (mid < right)
                    merge_runs_tiled(src, dst, left, mid, right);
                else
                    for (size_t i = left; i <= right; i++)
                        dst[i] = src[i];
            }
        }

        int32_t *tmp_ptr = src;
        src = dst;
        dst = tmp_ptr;
    }

    if (src != arr) {
#pragma omp parallel for schedule(static)
        for (size_t i = 0; i < n; i++)
            arr[i] = src[i];
    }

    free(tmp);
}

void sort_array(int32_t *arr, size_t size) {
    mergeSort_seq_dram_parallel(arr, size);
}

// main 
int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage: %s <input_binary_file>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];

    size_t size;
    int32_t *input_arr = load_data(filename, &size);

    int32_t *sorted_arr = allocate_aligned(size);
    memcpy(sorted_arr, input_arr, size * sizeof(int32_t));

    double t0 = omp_get_wtime();
    sort_array(sorted_arr, size);
    double t1 = omp_get_wtime();

    printf("Sorting time: %.6f sec\n", t1 - t0);
    printf("Verification: %s\n", verify_sorted(sorted_arr, size) ? "Sorted" : "NOT sorted");
    print_memory_usage();

    free(input_arr);
    free(sorted_arr);
    return 0;
}