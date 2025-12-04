// tile_merge_sort.c
// Tile-based cache-optimized merge sort for int32 data.
//
// 编译：gcc -O3 -march=native tile_merge_sort.c -o tile_merge
// 运行：./tile_merge input.bin
//
// input.bin: 按你的 hw4 一样，里面是连续的 int（4 字节）二进制数据

#define _POSIX_C_SOURCE 200112L  // for posix_memalign

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define CACHE_LINE_SIZE 64

// 这里按 L1 = 32KB 粗略估计：
// 每次 tile 只放 2048 个 int（8KB），L 和 R 两个 tile 一共 16KB，
// 再加上一点 arr 的写入缓冲，基本可以稳稳放进 L1。
#define TILE_ELEMS 2048          // 一个 tile 里的元素个数（可调）

#define INSERTION_THRESHOLD 32   // 小数组用插入排序

// 简单 min 宏
#define MIN(a,b) (( (a) < (b) ) ? (a) : (b))

// ====================== 内存对齐分配 =========================
static int* allocate_aligned(size_t count) {
    void* ptr = NULL;
    size_t size_in_bytes = count * sizeof(int);
    if (posix_memalign(&ptr, CACHE_LINE_SIZE, size_in_bytes) != 0) {
        fprintf(stderr, "posix_memalign failed\n");
        exit(EXIT_FAILURE);
    }
    return (int*)ptr;
}

// ====================== 插入排序（小数组用） ==================
static void insertion_sort(int* arr, int left, int right) {
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

// ============================================================
// 核心：Tile-based merge
//
// 思路：
// 1. 先把 arr[left..mid] 和 arr[mid+1..right] 拷到一个大 temp 里：L 和 R。
// 2. 再从 L / R 中每次取一小块（tile）到 bufL / bufR。
// 3. 只在当前这对 tile 上做 merge；某个 tile 用完就从 L/R 里再装下一块。
// 4. 始终只保留小块在 L1 中，达到真正的 cache blocking。
// ============================================================

static void merge_tiled(int* arr, int left, int mid, int right, int* temp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int* Lsrc = temp;         // L 的完整缓冲
    int* Rsrc = temp + n1;    // R 的完整缓冲

    // 一次性把整个 L 和 R 拷到临时缓冲里
    memcpy(Lsrc, &arr[left], n1 * sizeof(int));
    memcpy(Rsrc, &arr[mid + 1], n2 * sizeof(int));

    // Tile 缓冲放在栈上（2048 * 2 * 4 字节 ≈ 16KB）
    int bufL[TILE_ELEMS];
    int bufR[TILE_ELEMS];

    int i_src = 0;  // Lsrc 中读到哪了
    int j_src = 0;  // Rsrc 中读到哪了
    int lenL = 0;   // 当前 L tile 实际长度
    int lenR = 0;   // 当前 R tile 实际长度
    int i = 0;      // 当前在 bufL 中的位置
    int j = 0;      // 当前在 bufR 中的位置

    int k = left;   // 写回 arr 的位置

    // 辅助函数：装载下一个 L tile
    auto void refill_L(void) {
        if (i_src >= n1) {
            lenL = 0;  // 没有更多 L 数据
            return;
        }
        lenL = MIN(TILE_ELEMS, n1 - i_src);
        memcpy(bufL, Lsrc + i_src, lenL * sizeof(int));
        i_src += lenL;
        i = 0;
    };

    // 辅助函数：装载下一个 R tile
    auto void refill_R(void) {
        if (j_src >= n2) {
            lenR = 0;  // 没有更多 R 数据
            return;
        }
        lenR = MIN(TILE_ELEMS, n2 - j_src);
        memcpy(bufR, Rsrc + j_src, lenR * sizeof(int));
        j_src += lenR;
        j = 0;
    };

    // 初始化先装一块
    refill_L();
    refill_R();

    // 当 L 和 R 都还有 tile 时，做 tile × tile merge
    while (lenL > 0 && lenR > 0) {
        // 在当前这对 tiles 中做标准 merge
        while (i < lenL && j < lenR) {
            if (bufL[i] <= bufR[j]) {
                arr[k++] = bufL[i++];
                if (i == lenL) {  // 当前 L tile 用完，装下一块
                    refill_L();
                    if (lenL == 0) break; // L 完全没数据了
                }
            }
            else {
                arr[k++] = bufR[j++];
                if (j == lenR) {  // 当前 R tile 用完，装下一块
                    refill_R();
                    if (lenR == 0) break; // R 完全没数据了
                }
            }
        }
        // 如果其中一边 tile 用完，这一轮大 while 会重新判断 lenL/lenR 决定是否继续
    }

    // 到这里：要么 L 全部用完，要么 R 全部用完，甚至两者都用完。

    // 如果 L 还有剩余（可能在 bufL 中、也可能还在 Lsrc）
    while (lenL > 0) {
        while (i < lenL) {
            arr[k++] = bufL[i++];
        }
        refill_L();
    }
    // 如果 R 还有剩余
    while (lenR > 0) {
        while (j < lenR) {
            arr[k++] = bufR[j++];
        }
        refill_R();
    }

    // 此时如果 Lsrc 或 Rsrc 还有没读取到 tile 里的元素（理论上不会，多重 while 已覆盖），
    // 保险起见也可以直接补一发 memcpy：
    if (i_src < n1) {
        memcpy(&arr[k], &Lsrc[i_src], (n1 - i_src) * sizeof(int));
        k += (n1 - i_src);
        i_src = n1;
    }
    if (j_src < n2) {
        memcpy(&arr[k], &Rsrc[j_src], (n2 - j_src) * sizeof(int));
        k += (n2 - j_src);
        j_src = n2;
    }
}

// ====================== 递归 Merge Sort（用 tiled merge） =================

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

    // 用 tile-based merge 替代普通 merge
    merge_tiled(arr, left, mid, right, temp);
}

static void mergeSort_tiled(int* arr, size_t n) {
    int* temp = allocate_aligned(n);   // 临时缓冲，放 L 和 R
    mergeSort_tiled_rec(arr, 0, (int)n - 1, temp);
    free(temp);
}

// ====================== 数据加载 / 校验 / main ===========================

static int* load_data(const char* filename, size_t* size_out) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (bytes < 0 || bytes % sizeof(int) != 0) {
        fprintf(stderr, "Invalid file size\n");
        exit(EXIT_FAILURE);
    }

    size_t n = (size_t)(bytes / sizeof(int));
    int* arr = allocate_aligned(n);

    size_t read_count = fread(arr, sizeof(int), n, f);
    if (read_count != n) {
        fprintf(stderr, "Failed to read all data\n");
        exit(EXIT_FAILURE);
    }

    fclose(f);
    *size_out = n;
    return arr;
}

static int verify_sorted(const int* arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        if (arr[i] < arr[i - 1]) return 0;
    }
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_binary_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    size_t n = 0;
    printf("Loading data from %s...\n", argv[1]);
    int* arr = load_data(argv[1], &n);
    printf("Loaded %zu integers\n", n);

    clock_t start = clock();
    mergeSort_tiled(arr, n);
    clock_t end = clock();

    double sec = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Sorting finished in %.4f seconds\n", sec);

    if (verify_sorted(arr, n)) {
        printf("Verification: sorted.\n");
    }
    else {
        printf("Verification: NOT sorted!\n");
    }

    double gb = (n * sizeof(int)) / (1024.0 * 1024.0 * 1024.0);
    double thr = gb / sec;
    printf("Data size: %.4f GB, Throughput: %.4f GB/s\n", gb, thr);

    free(arr);
    return 0;
}
