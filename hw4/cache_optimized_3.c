// cache_optimized_advanced.c - 真正的高级优化版本
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define CACHE_LINE_SIZE 64
#define L1_CACHE_SIZE 32768        // 32KB
#define L2_CACHE_SIZE 262144       // 256KB
#define PREFETCH_DISTANCE 16       // 预取距离

#define ALIGN_TO_CACHELINE(x) (((x) + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1))

// 内存对齐分配
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

// ============================================
// 关键优化1: 软件预取 + 循环展开
// ============================================
void merge_with_prefetch_unroll(int* arr, int left, int mid, int right, int* temp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int* L = temp;
    int* R = temp + n1;

    // 使用非临时存储提示（streaming store）
    memcpy(L, &arr[left], n1 * sizeof(int));
    memcpy(R, &arr[mid + 1], n2 * sizeof(int));

    int i = 0, j = 0, k = left;

    // 主循环：4路展开 + 软件预取
    while (i + 3 < n1 && j + 3 < n2) {
        // 预取未来的数据
        __builtin_prefetch(&L[i + PREFETCH_DISTANCE], 0, 3);
        __builtin_prefetch(&R[j + PREFETCH_DISTANCE], 0, 3);
        __builtin_prefetch(&arr[k + PREFETCH_DISTANCE], 1, 3);

        // 4路循环展开
        for (int unroll = 0; unroll < 4; unroll++) {
            if (L[i] <= R[j]) {
                arr[k++] = L[i++];
            }
            else {
                arr[k++] = R[j++];
            }
        }
    }

    // 处理剩余元素
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k++] = L[i++];
        }
        else {
            arr[k++] = R[j++];
        }
    }

    // 使用 memcpy 复制剩余（比循环快）
    if (i < n1) {
        memcpy(&arr[k], &L[i], (n1 - i) * sizeof(int));
    }
    if (j < n2) {
        memcpy(&arr[k], &R[j], (n2 - j) * sizeof(int));
    }
}

// ============================================
// 关键优化2: 自适应分块策略
// ============================================
void merge_adaptive_blocking(int* arr, int left, int mid, int right, int* temp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;
    int total_size = (n1 + n2) * sizeof(int);

    int* L = temp;
    int* R = temp + n1;

    memcpy(L, &arr[left], n1 * sizeof(int));
    memcpy(R, &arr[mid + 1], n2 * sizeof(int));

    int i = 0, j = 0, k = left;

    // 根据数据大小自适应选择块大小
    int block_size;
    if (total_size <= L1_CACHE_SIZE / 2) {
        // 小数据：适合 L1 缓存
        block_size = 256;  // 1KB
    }
    else if (total_size <= L2_CACHE_SIZE / 2) {
        // 中等数据：适合 L2 缓存
        block_size = 4096; // 16KB
    }
    else {
        // 大数据：L3 缓存或内存
        block_size = 16384; // 64KB
    }

    // 分块归并
    while (i < n1 && j < n2) {
        int i_end = (i + block_size < n1) ? i + block_size : n1;
        int j_end = (j + block_size < n2) ? j + block_size : n2;

        // 在这个块内密集处理（保持在缓存中）
        while (i < i_end && j < j_end) {
            // 预取
            if (i + PREFETCH_DISTANCE < i_end) {
                __builtin_prefetch(&L[i + PREFETCH_DISTANCE], 0, 1);
            }
            if (j + PREFETCH_DISTANCE < j_end) {
                __builtin_prefetch(&R[j + PREFETCH_DISTANCE], 0, 1);
            }

            if (L[i] <= R[j]) {
                arr[k++] = L[i++];
            }
            else {
                arr[k++] = R[j++];
            }
        }

        // 处理块边界的剩余元素
        while (i < i_end && j < n2) {
            if (L[i] <= R[j]) {
                arr[k++] = L[i++];
            }
            else {
                arr[k++] = R[j++];
            }
        }

        while (j < j_end && i < n1) {
            if (L[i] <= R[j]) {
                arr[k++] = L[i++];
            }
            else {
                arr[k++] = R[j++];
            }
        }
    }

    if (i < n1) {
        memcpy(&arr[k], &L[i], (n1 - i) * sizeof(int));
    }
    if (j < n2) {
        memcpy(&arr[k], &R[j], (n2 - j) * sizeof(int));
    }
}

// ============================================
// 关键优化3: 小数组优化 - 插入排序
// ============================================
void insertion_sort(int* arr, int left, int right) {
    for (int i = left + 1; i <= right; i++) {
        int key = arr[i];
        int j = i - 1;

        while (j >= left && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// ============================================
// 关键优化4: 混合排序策略
// ============================================
#define INSERTION_THRESHOLD 32  // 小于32个元素用插入排序

void mergeSort_hybrid(int* arr, int left, int right, int* temp) {
    if (left >= right) return;

    int n = right - left + 1;

    // 小数组使用插入排序（避免递归开销）
    if (n <= INSERTION_THRESHOLD) {
        insertion_sort(arr, left, right);
        return;
    }

    int mid = left + (right - left) / 2;

    mergeSort_hybrid(arr, left, mid, temp);
    mergeSort_hybrid(arr, mid + 1, right, temp);

    // 根据数据大小选择不同的归并策略
    if (n * sizeof(int) < L1_CACHE_SIZE) {
        // 小规模：预取 + 展开
        merge_with_prefetch_unroll(arr, left, mid, right, temp);
    }
    else {
        // 大规模：自适应分块
        merge_adaptive_blocking(arr, left, mid, right, temp);
    }
}

void mergeSort_wrapper(int* arr, int n) {
    int* temp = allocate_aligned(n);
    mergeSort_hybrid(arr, 0, n - 1, temp);
    free(temp);
}

// ============================================
// 测试代码
// ============================================
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

    printf("Starting advanced cache-optimized merge sort...\n");
    clock_t start = clock();

    mergeSort_wrapper(arr, size);

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Sorting completed in %.4f seconds\n", time_spent);

    if (verify_sorted(arr, size)) {
        printf("Verification: Array is correctly sorted!\n");
    }
    else {
        printf("Verification: ERROR - Array is NOT sorted!\n");
    }

    double gb_sorted = (size * sizeof(int)) / (1024.0 * 1024.0 * 1024.0);
    double throughput = gb_sorted / time_spent;
    printf("Data size: %.4f GB\n", gb_sorted);
    printf("Throughput: %.4f GB/s\n", throughput);

    free(arr);
    return EXIT_SUCCESS;
}