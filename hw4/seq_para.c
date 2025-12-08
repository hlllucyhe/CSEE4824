#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/resource.h>
#include <omp.h>

// --------- CONFIG ----------
#define OMP_THRESHOLD (1 << 10)  

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

// --------- load_data ----------
int *load_data(const char *filename, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *size = file_size / sizeof(int);

    int *arr = (int *)malloc(*size * sizeof(int));
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

// --------- verify ----------
int verify_sorted(const int *arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (arr[i] < arr[i - 1]) return 0;
    }
    return 1;
}

// --------- merge (DRAM-friendly sequential merge) ----------
static void merge_runs(const int* src, int* dst, int left, int mid, int right) {
    int i = left;
    int j = mid + 1;
    int k = left;

    while (i <= mid && j <= right) {
        if (src[i] <= src[j]) dst[k++] = src[i++];
        else                  dst[k++] = src[j++];
    }

    while (i <= mid)  dst[k++] = src[i++];
    while (j <= right) dst[k++] = src[j++];
}

// --------- DRAM-friendly Bottom-up MergeSort + OpenMP ----------
void mergeSort_seq_dram_parallel(int *arr, size_t n) {
    int *tmp = (int *)malloc(n * sizeof(int));
    if (!tmp) {
        perror("malloc tmp failed");
        exit(EXIT_FAILURE);
    }

    int *src = arr;
    int *dst = tmp;

    for (size_t width = 1; width < n; width *= 2) {

        // big run parallel，small run serially（avoid thread overhead）
        if (2 * width >= OMP_THRESHOLD) {

#pragma omp parallel for schedule(static)
            for (size_t left = 0; left < n; left += 2 * width) {
                size_t mid   = left + width - 1;
                size_t right = left + 2 * width - 1;

                if (mid >= n)   mid = n - 1;
                if (right >= n) right = n - 1;

                if (mid < right)
                    merge_runs(src, dst, left, mid, right);
                else {
                    for (size_t i = left; i <= right; i++)
                        dst[i] = src[i];
                }
            }

        } else {

            // 串行
            for (size_t left = 0; left < n; left += 2 * width) {
                size_t mid   = left + width - 1;
                size_t right = left + 2 * width - 1;

                if (mid >= n)   mid = n - 1;
                if (right >= n) right = n - 1;

                if (mid < right)
                    merge_runs(src, dst, left, mid, right);
                else {
                    for (size_t i = left; i <= right; i++)
                        dst[i] = src[i];
                }
            }
        }

        // swap src/dst
        int *tmp_ptr = src;
        src = dst;
        dst = tmp_ptr;
    }

    // make sure the result is in arr
    if (src != arr) {
#pragma omp parallel for schedule(static)
        for (size_t i = 0; i < n; i++)
            arr[i] = src[i];
    }

    free(tmp);
}

// main
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

    // throughput
    double gb_sorted = (n * sizeof(int)) / (1024.0 * 1024.0 * 1024.0);
    printf("Sorted data: %.4f GB\n", gb_sorted);
    printf("Throughput: %.4f GB/s\n", gb_sorted / duration);

    // verification
    printf("Verification: %s\n",
           verify_sorted(arr, n) ? "Sorted" : "NOT sorted");

    free(arr);
    return 0;
}