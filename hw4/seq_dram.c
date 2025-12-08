#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/resource.h>

// --------- Peak memory ----------
void print_memory_usage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("Max RSS: %ld kB (%.2f MB)\n",
           usage.ru_maxrss,
           usage.ru_maxrss / 1024.0);
}

// --------- Timing ----------
double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

// --------- load data from file ----------
int *load_data(const char *filename, size_t *size_out) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *size_out = file_size / sizeof(int);

    int *arr = (int *)malloc(*size_out * sizeof(int));
    if (!arr) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    size_t read_count = fread(arr, sizeof(int), *size_out, file);
    if (read_count != *size_out) {
        fprintf(stderr, "Incomplete data read\n");
        exit(EXIT_FAILURE);
    }

    fclose(file);
    return arr;
}

// --------- verify sorted ----------
int verify_sorted(const int *arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (arr[i] < arr[i - 1]) return 0;
    }
    return 1;
}

// --------- merge two runs ----------
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

// --------- DRAM-friendly bottom-up merge sort ----------
void mergeSort_seq(int* arr, size_t n) {
    int *tmp = (int*)malloc(n * sizeof(int));
    if (!tmp) {
        perror("malloc failed");
        exit(1);
    }

    int *src = arr;
    int *dst = tmp;

    for (size_t width = 1; width < n; width *= 2) {
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

        // swap src and dst
        int *tmp_ptr = src;
        src = dst;
        dst = tmp_ptr;
    }

    // ensure result is in arr
    if (src != arr) {
        for (size_t i = 0; i < n; i++)
            arr[i] = src[i];
    }

    free(tmp);
}

// --------- main: now uses file input ----------
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    size_t n;
    printf("Loading data from %s...\n", argv[1]);
    int *arr = load_data(argv[1], &n);
    printf("Loaded %zu integers\n", n);

    double start = get_time_ms();
    mergeSort_seq(arr, n);
    double end = get_time_ms();

    printf("Sequential DRAM MergeSort Time: %.3f ms\n", end - start);
    print_memory_usage();

    printf("Verification: %s\n",
           verify_sorted(arr, n) ? "sorted" : "NOT sorted");

    free(arr);
    return 0;
}