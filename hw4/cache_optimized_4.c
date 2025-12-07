// tile_merge_sort.c
// Tile-based cache-optimized merge sort for int32 data.


#define _POSIX_C_SOURCE 200112L  // for posix_memalign

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define CACHE_LINE_SIZE 64


#define TILE_ELEMS 2048         

#define INSERTION_THRESHOLD 32  


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



static void merge_tiled(int* arr, int left, int mid, int right, int* temp) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int* Lsrc = temp;        
    int* Rsrc = temp + n1;    


    memcpy(Lsrc, &arr[left], n1 * sizeof(int));
    memcpy(Rsrc, &arr[mid + 1], n2 * sizeof(int));

 
    int bufL[TILE_ELEMS];
    int bufR[TILE_ELEMS];

    int i_src = 0;  
    int j_src = 0;  
    int lenL = 0;   
    int lenR = 0;   
    int i = 0;      
    int j = 0;      

    int k = left;   


    auto void refill_L(void) {
        if (i_src >= n1) {
            lenL = 0;  
            return;
        }
        lenL = MIN(TILE_ELEMS, n1 - i_src);
        memcpy(bufL, Lsrc + i_src, lenL * sizeof(int));
        i_src += lenL;
        i = 0;
    };


    auto void refill_R(void) {
        if (j_src >= n2) {
            lenR = 0;  
            return;
        }
        lenR = MIN(TILE_ELEMS, n2 - j_src);
        memcpy(bufR, Rsrc + j_src, lenR * sizeof(int));
        j_src += lenR;
        j = 0;
    };


    refill_L();
    refill_R();

   
    while (lenL > 0 && lenR > 0) {
  
        while (i < lenL && j < lenR) {
            if (bufL[i] <= bufR[j]) {
                arr[k++] = bufL[i++];
                if (i == lenL) { 
                    refill_L();
                    if (lenL == 0) break;
                }
            }
            else {
                arr[k++] = bufR[j++];
                if (j == lenR) {  
                    refill_R();
                    if (lenR == 0) break;
                }
            }
        }
 
    }


    while (lenL > 0) {
        while (i < lenL) {
            arr[k++] = bufL[i++];
        }
        refill_L();
    }

    while (lenR > 0) {
        while (j < lenR) {
            arr[k++] = bufR[j++];
        }
        refill_R();
    }

  
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


    merge_tiled(arr, left, mid, right, temp);
}

static void mergeSort_tiled(int* arr, size_t n) {
    int* temp = allocate_aligned(n);  
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
