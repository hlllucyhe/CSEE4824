#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define THRESHOLD  5000   // only parallelize when (right-left+1) >= THRESHOLD

// Your baseline merge for int arrays (unchanged)
void merge(int* arr, int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int* L = (int*)malloc(n1 * sizeof(int));
    int* R = (int*)malloc(n2 * sizeof(int));
    if (!L || !R) {
        fprintf(stderr, "malloc failed in merge\n");
        exit(1);
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

// Serial version (used for small subproblems)
void mergeSort_serial(int* arr, int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;

        mergeSort_serial(arr, left, mid);
        mergeSort_serial(arr, mid + 1, right);

        merge(arr, left, mid, right);
    }
}

// Parallel version using OpenMP tasks
void mergeSort_parallel(int* arr, int left, int right) {
    if (left >= right) return;

    int n = right - left + 1;

    if (n <= THRESHOLD) {
        // For small segments, just do serial mergesort
        mergeSort_serial(arr, left, right);
        return;
    }

    int mid = left + (right - left) / 2;

    // Create tasks for the two halves
    #pragma omp task shared(arr)
    mergeSort_parallel(arr, left, mid);

    #pragma omp task shared(arr)
    mergeSort_parallel(arr, mid + 1, right);

    #pragma omp taskwait
    merge(arr, left, mid, right);
}

int main() {
    int arr[] = {38, 27, 43, 3, 9, 82, 10};
    int arr_size = sizeof(arr) / sizeof(arr[0]);

    printf("Given array is \n");
    for (int i = 0; i < arr_size; i++)
        printf("%d ", arr[i]);
    printf("\n");

    // Parallel region with a single initial task
    #pragma omp parallel
    {
        #pragma omp single
        {
            mergeSort_parallel(arr, 0, arr_size - 1);
        }
    }

    printf("\nSorted array is \n");
    for (int i = 0; i < arr_size; i++)
        printf("%d ", arr[i]);
    printf("\n");
    return 0;
}
