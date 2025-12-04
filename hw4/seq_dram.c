#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// timing
double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

// merge run：
// src[left..mid] 和 src[mid+1..right] -> write dst[left..right]
static void merge_runs(const int* src, int* dst, int left, int mid, int right) {
    int i = left;      // right run 
    int j = mid + 1;   // left run 
    int k = left;      // dst 

    // if two run both have elements, merge sequentially
    while (i <= mid && j <= right) {
        if (src[i] <= src[j]) {      
            dst[k++] = src[i++];
        } else {
            dst[k++] = src[j++];
        }
    }

    // store the rest run (only one left)
    while (i <= mid) {
        dst[k++] = src[i++];
    }
    while (j <= right) {
        dst[k++] = src[j++];
    }
}

// from bottom to the top, sequentilly dram merge sort
void mergeSort_seq(int* arr, int n) {
    // apply for a buffer with the same size of array
    int* tmp = (int*)malloc(n * sizeof(int));
    if (!tmp) {
        perror("malloc failed");
        exit(1);
    }

    int* src = arr;
    int* dst = tmp;

    // width is the legth of each sorted run，initial 1
    for (int width = 1; width < n; width *= 2) {

        // from left to the right, merge run pairly
        for (int left = 0; left < n; left += 2 * width) {
            int mid  = left + width - 1;
            int right = left + 2 * width - 1;

            //process the rest run
            if (mid >= n) {
                mid = n - 1;
            }
            if (right >= n) {
                right = n - 1;
            }

            if (mid < right) {
                // both of them exist：src[left..mid] and src[mid+1..right]
                merge_runs(src, dst, left, mid, right);
            } else {
                // only one run（legth < 2*width），do copy
                for (int i = left; i <= right; i++) {
                    dst[i] = src[i];
                }
            }
        }

        // one round end：exchange src and dst, do the next round
        int* tmp_ptr = src;
        src = dst;
        dst = tmp_ptr;
    }

    // if not in arr at the end, but in tmp, copy it back.
    if (src != arr) {
        for (int i = 0; i < n; i++) {
            arr[i] = src[i];
        }
    }

    free(tmp);
}

int main() {
    int n = 1 << 22;   // 4M 
    int* arr = malloc(n * sizeof(int));

    // random data
    for (int i = 0; i < n; i++) arr[i] = rand();
 

    // timing 
    double start = get_time_ms();
    mergeSort_seq(arr, n);
    double end = get_time_ms();

    printf("Sequential DRAM MergeSort Time: %.3f ms\n", end - start);

    free(arr);
    return 0;
}