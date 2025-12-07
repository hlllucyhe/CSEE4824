// baseline.c - serial merge sort with timing and RSS measurement
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/resource.h>

// ---------- Timing helper (wall-clock) ----------
double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// ---------- Peak RSS helper ----------
void print_memory_usage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("Max RSS: %ld kB (%.2f MB)\n",
           usage.ru_maxrss,
           usage.ru_maxrss / 1024.0);
}

// ---------- Merge sort ----------
void merge(int *arr, int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int *L = (int *)malloc(n1 * sizeof(int));
    int *R = (int *)malloc(n2 * sizeof(int));
    if (!L || !R) {
        fprintf(stderr, "malloc failed in merge\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n1; i++)
        L[i] = arr[left + i];
    for (int j = 0; j < n2; j++)
        R[j] = arr[mid + 1 + j];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k++] = L[i++];
        } else {
            arr[k++] = R[j++];
        }
    }

    while (i < n1) {
        arr[k++] = L[i++];
    }
    while (j < n2) {
        arr[k++] = R[j++];
    }

    free(L);
    free(R);
}

void mergeSort(int *arr, int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        mergeSort(arr, left, mid);
        mergeSort(arr, mid + 1, right);
        merge(arr, left, mid, right);
    }
}

// ---------- I/O + verification ----------
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
        fprintf(stderr, "Failed to read complete data\n");
        exit(EXIT_FAILURE);
    }

    fclose(file);
    return arr;
}

int verify_sorted(int *arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (arr[i] < arr[i - 1]) {
            return 0;
        }
    }
    return 1;
}

// ---------- main ----------
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    size_t size;

    printf("Loading data from %s...\n", filename);
    int *arr = load_data(filename, &size);
    printf("Loaded %zu integers\n", size);

    printf("Starting baseline (serial) merge sort...\n");
    double start = now_sec();

    mergeSort(arr, 0, (int)size - 1);

    double end = now_sec();
    double time_spent = end - start;

    printf("Sorting completed in %.6f seconds\n", time_spent);
    print_memory_usage();

    if (verify_sorted(arr, size)) {
        printf("Verification: Array is correctly sorted!\n");
    } else {
        printf("Verification: ERROR - Array is NOT sorted!\n");
    }

    double gb_sorted = (size * sizeof(int)) / (1024.0 * 1024.0 * 1024.0);
    double throughput = gb_sorted / time_spent;
    printf("Data size: %.4f GB\n", gb_sorted);
    printf("Throughput: %.4f GB/s\n", throughput);

    free(arr);
    return EXIT_SUCCESS;
}
