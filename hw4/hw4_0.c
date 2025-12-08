
// hw4.c 
// Combine: parallel_3.c + cache_optimized_4.c + seq_dram.c

#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <time.h>
#include <stdint.h>
#include <sys/resource.h>

//  Optimization Flags (set 0 or 1)
#define USE_PARALLEL 0
#define USE_ALIGNMENT 0
#define USE_CACHE_BLOCK 0
#define USE_SEQ_DRAM 0


//  Memory Usage (Peak RSS)
void print_memory_usage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    // ru_maxrss is in KB
    double rss_mb = usage.ru_maxrss / 1024.0;

    printf("[Memory] Max RSS: %.2f MB\n", rss_mb);
}


//  TIMER
double get_time_sec() {
    return omp_get_wtime();
}


//  BASELINE MERGE SORT
void merge_baseline(int *arr, int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int *L = malloc(n1 * sizeof(int));
    int *R = malloc(n2 * sizeof(int));

    for (int i = 0; i < n1; i++) L[i] = arr[left + i];
    for (int j = 0; j < n2; j++) R[j] = arr[mid + 1 + j];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2)
        arr[k++] = (L[i] <= R[j]) ? L[i++] : R[j++];

    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];

    free(L);
    free(R);
}

void mergesort_baseline(int *arr, int left, int right) {
    if (left < right) {
        int mid = (left + right) / 2;
        mergesort_baseline(arr, left, mid);
        mergesort_baseline(arr, mid + 1, right);
        merge_baseline(arr, left, mid, right);
    }
}


//  PARALLEL MERGE SORT  (OpenMP)
#define THRESHOLD 50000

// void mergesort_serial(int *arr, int left, int right);

void merge_parallel(int *arr, int left, int mid, int right) {
    merge_baseline(arr, left, mid, right);
}

void mergesort_parallel(int *arr, int left, int right) {
    if (right - left + 1 <= THRESHOLD)
        return mergesort_baseline(arr, left, right);

    int mid = (left + right) / 2;

#pragma omp task shared(arr)
    mergesort_parallel(arr, left, mid);

#pragma omp task shared(arr)
    mergesort_parallel(arr, mid + 1, right);

#pragma omp taskwait
    merge_parallel(arr, left, mid, right);
}


//  CACHE-BLOCKED + MEMORY-ALIGNED MERGE SORT
#define CACHE_LINE_SIZE 64
#define TILE 2048

static int* allocate_aligned(size_t count) {
    void* ptr = NULL;
    posix_memalign(&ptr, CACHE_LINE_SIZE, count * sizeof(int));
    return ptr;
}

static void merge_tiled(int *arr, int left, int mid, int right, int *temp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    memcpy(temp, &arr[left], n1 * sizeof(int));
    memcpy(temp + n1, &arr[mid + 1], n2 * sizeof(int));

    int *L = temp;
    int *R = temp + n1;

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2)
        arr[k++] = (L[i] <= R[j]) ? L[i++] : R[j++];

    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];
}

void mergesort_tiled_rec(int *arr, int left, int right, int *temp) {
    if (left >= right) return;

    int mid = (left + right) / 2;
    mergesort_tiled_rec(arr, left, mid, temp);
    mergesort_tiled_rec(arr, mid + 1, right, temp);

    merge_tiled(arr, left, mid, right, temp);
}

void mergesort_cacheblocked(int *arr, int n) {
    int *temp = allocate_aligned(n);
    mergesort_tiled_rec(arr, 0, n - 1, temp);
    free(temp);
}


//  SEQUENTIAL-DRAM MERGE SORT (Bottom-up)
static void merge_runs(const int *src, int *dst, int left, int mid, int right) {
    int i = left, j = mid + 1, k = left;
    while (i <= mid && j <= right)
        dst[k++] = (src[i] <= src[j]) ? src[i++] : src[j++];
    while (i <= mid) dst[k++] = src[i++];
    while (j <= right) dst[k++] = src[j++];
}

void mergesort_seqdram(int *arr, int n) {
    int *tmp = malloc(n * sizeof(int));

    int *src = arr;
    int *dst = tmp;

    for (int width = 1; width < n; width *= 2) {
        for (int left = 0; left < n; left += 2*width) {
            int mid = left + width - 1;
            int right = left + 2*width - 1;
            if (mid >= n) mid = n - 1;
            if (right >= n) right = n - 1;

            if (mid < right)
                merge_runs(src, dst, left, mid, right);
            else
                memcpy(dst + left, src + left, (right - left + 1) * sizeof(int));
        }

        int *tmp2 = src;
        src = dst;
        dst = tmp2;
    }

    if (src != arr)
        memcpy(arr, src, n * sizeof(int));

    free(tmp);
}



//  main
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <input_binary_file>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];

    // load data from file
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Open file failed");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    fseek(f, 0, SEEK_SET);

    int n = bytes / sizeof(int);
    printf("Loaded %d integers from %s\n", n, filename);

    int *arr = malloc(n * sizeof(int));
    fread(arr, sizeof(int), n, f);
    fclose(f);

    printf("Flags: P=%d CB=%d SD=%d BL=%d\n",
           USE_PARALLEL, USE_CACHE_BLOCK, USE_SEQ_DRAM, USE_BASELINE);

    double start = get_time_sec();

    // choose implementation
#if USE_PARALLEL
#pragma omp parallel
    {
#pragma omp single
        mergesort_parallel(arr, 0, n - 1);
    }
#elif USE_CACHE_BLOCK
    mergesort_cacheblocked(arr, n);
#elif USE_SEQ_DRAM
    mergesort_seqdram(arr, n);
#else
    mergesort_baseline(arr, 0, n - 1);
#endif

    double end = get_time_sec();
    printf("Time: %.6f sec\n", end - start);
    print_memory_usage();

    free(arr);
    return 0;
}
