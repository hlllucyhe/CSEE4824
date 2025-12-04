// cache_optimized_v2.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CACHE_LINE_SIZE 64
#define L1_CACHE_SIZE 32768        // 32KB L1 cache
#define L2_CACHE_SIZE 262144       // 256KB L2 cache
#define L1_BLOCK_SIZE 4096         // 4KB blocks for L1
#define L2_BLOCK_SIZE 32768        // 32KB blocks for L2

#define ALIGN_TO_CACHELINE(x) (((x) + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1))

// 对齐分配
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
// 优化1: 真正的缓存分块归并
// ============================================
void merge_cache_blocked_v2(int* arr, int left, int mid, int right, int* temp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int* L = temp;
    int* R = temp + n1;

    // 使用 memcpy 复制数据（已经是优化）
    memcpy(L, &arr[left], n1 * sizeof(int));
    memcpy(R, &arr[mid + 1], n2 * sizeof(int));

    int i = 0, j = 0, k = left;

    // 关键优化：按块处理，每次处理 L1_BLOCK_SIZE/sizeof(int) 个元素
    int block_size = L1_BLOCK_SIZE / sizeof(int);  // 每块1024个整数

    while (i < n1 && j < n2) {
        // 确定当前块的边界
        int i_end = (i + block_size < n1) ? i + block_size : n1;
        int j_end = (j + block_size < n2) ? j + block_size : n2;

        // 在这个块内进行归并（数据保持在L1缓存中）
        while (i < i_end && j < j_end) {
            if (L[i] <= R[j]) {
                arr[k++] = L[i++];
            }
            else {
                arr[k++] = R[j++];
            }
        }

        // 处理剩余的块边界元素
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

    // 复制剩余元素
    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];
}

// ============================================
// 优化2: 软件预取（Prefetching）
// ============================================
void merge_with_prefetch(int* arr, int left, int mid, int right, int* temp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int* L = temp;
    int* R = temp + n1;

    memcpy(L, &arr[left], n1 * sizeof(int));
    memcpy(R, &arr[mid + 1], n2 * sizeof(int));

    int i = 0, j = 0, k = left;

    // 预取距离：提前16个元素预取
#define PREFETCH_DISTANCE 16

    while (i < n1 && j < n2) {
        // 软件预取：告诉CPU提前加载数据到缓存
        if (i + PREFETCH_DISTANCE < n1) {
            __builtin_prefetch(&L[i + PREFETCH_DISTANCE], 0, 1);
        }
        if (j + PREFETCH_DISTANCE < n2) {
            __builtin_prefetch(&R[j + PREFETCH_DISTANCE], 0, 1);
        }
        if (k + PREFETCH_DISTANCE < right) {
            __builtin_prefetch(&arr[k + PREFETCH_DISTANCE], 1, 1);
        }

        if (L[i] <= R[j]) {
            arr[k++] = L[i++];
        }
        else {
            arr[k++] = R[j++];
        }
    }

    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];
}

// ============================================
// 优化3: 多级缓存分块
// ============================================
void merge_multilevel_cache_blocked(int* arr, int left, int mid, int right, int* temp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;
    int total_size = n1 + n2;

    int* L = temp;
    int* R = temp + n1;

    memcpy(L, &arr[left], n1 * sizeof(int));
    memcpy(R, &arr[mid + 1], n2 * sizeof(int));

    int i = 0, j = 0, k = left;

    // 根据数据大小选择不同的块大小
    int block_size;
    if (total_size * sizeof(int) < L1_CACHE_SIZE / 2) {
        // 数据能放进L1，使用小块
        block_size = L1_BLOCK_SIZE / sizeof(int);
    }
    else if (total_size * sizeof(int) < L2_CACHE_SIZE / 2) {
        // 数据能放进L2，使用中等块
        block_size = L2_BLOCK_SIZE / sizeof(int);
    }
    else {
        // 数据太大，使用大块
        block_size = L2_CACHE_SIZE / sizeof(int);
    }

    while (i < n1 && j < n2) {
        int i_end = (i + block_size < n1) ? i + block_size : n1;
        int j_end = (j + block_size < n2) ? j + block_size : n2;

        while (i < i_end && j < j_end) {
            if (L[i] <= R[j]) {
                arr[k++] = L[i++];
            }
            else {
                arr[k++] = R[j++];
            }
        }
    }

    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];
}

// ============================================
// 优化4: SIMD 向量化归并（使用编译器内建函数）
// ============================================
#include <immintrin.h>  // SSE/AVX intrinsics

void merge_simd(int* arr, int left, int mid, int right, int* temp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int* L = temp;
    int* R = temp + n1;

    // 使用 AVX 指令加速复制（如果编译器支持）
    memcpy(L, &arr[left], n1 * sizeof(int));
    memcpy(R, &arr[mid + 1], n2 * sizeof(int));

    int i = 0, j = 0, k = left;

    // 标准归并（编译器可能会自动向量化）
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k++] = L[i++];
        }
        else {
            arr[k++] = R[j++];
        }
    }

    // 使用 memcpy 复制剩余元素（比循环快）
    if (i < n1) {
        memcpy(&arr[k], &L[i], (n1 - i) * sizeof(int));
        k += n1 - i;
    }
    if (j < n2) {
        memcpy(&arr[k], &R[j], (n2 - j) * sizeof(int));
    }
}

// ============================================
// 优化5: 使用 Huge Pages（大页内存）
// ============================================
#include <sys/mman.h>

int* allocate_huge_pages(size_t count) {
    size_t size = count * sizeof(int);
    // 对齐到2MB（huge page大小）
    size_t aligned_size = ((size + 2 * 1024 * 1024 - 1) / (2 * 1024 * 1024)) * (2 * 1024 * 1024);

    void* ptr = mmap(NULL, aligned_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
        -1, 0);

    if (ptr == MAP_FAILED) {
        // 如果huge pages失败，回退到普通内存
        ptr = mmap(NULL, aligned_size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1, 0);
    }

    if (ptr == MAP_FAILED) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    return (int*)ptr;
}

// ============================================
// 递归归并排序（选择不同的merge函数）
// ============================================
void mergeSort_optimized(int* arr, int left, int right, int* temp) {
    if (left < right) {
        int mid = left + (right - left) / 2;

        mergeSort_optimized(arr, left, mid, temp);
        mergeSort_optimized(arr, mid + 1, right, temp);

        // 选择你想测试的merge函数：
        // merge_cache_blocked_v2(arr, left, mid, right, temp);
        // merge_with_prefetch(arr, left, mid, right, temp);
        merge_multilevel_cache_blocked(arr, left, mid, right, temp);
        // merge_simd(arr, left, mid, right, temp);
    }
}

void mergeSort_wrapper(int* arr, int n) {
    int* temp = allocate_aligned(n);
    // 或者使用 huge pages:
    // int* temp = allocate_huge_pages(n);

    mergeSort_optimized(arr, 0, n - 1, temp);

    free(temp);
    // 或者: munmap(temp, size);
}

// ============================================
// 主函数和测试代码（与之前相同）
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

    printf("Starting cache-optimized merge sort (v2)...\n");
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