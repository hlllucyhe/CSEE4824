#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <immintrin.h>

#define MAX_DIGITS 2048
#define BASE 10

void printBigInt(int* num, int len) {
    int i = len - 1;
    while (i > 0 && num[i] == 0) i--;
    for (; i >= 0; i--)
        printf("%d", num[i]);
    printf("\n");
}

int stringToBigInt(const char* str, int* num) {
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        num[i] = str[len - 1 - i] - '0';
    }
    return len;
}

void multiply_vectorized_optimized(int* A, int lenA, int* B, int lenB, int* C) {
    memset(C, 0, sizeof(int) * (lenA + lenB));

    for (int i = 0; i < lenA; i++) {
        int a_val = A[i];
        __m256i a_vec = _mm256_set1_epi32(a_val);

        int j = 0;
        for (; j <= lenB - 16; j += 16) {
            __m256i b_vec1 = _mm256_loadu_si256((__m256i*) & B[j]);
            __m256i b_vec2 = _mm256_loadu_si256((__m256i*) & B[j + 8]);

            __m256i c_vec1 = _mm256_loadu_si256((__m256i*) & C[i + j]);
            __m256i c_vec2 = _mm256_loadu_si256((__m256i*) & C[i + j + 8]);

            __m256i product1 = _mm256_mullo_epi32(a_vec, b_vec1);
            __m256i product2 = _mm256_mullo_epi32(a_vec, b_vec2);

            __m256i sum1 = _mm256_add_epi32(c_vec1, product1);
            __m256i sum2 = _mm256_add_epi32(c_vec2, product2);

            _mm256_storeu_si256((__m256i*) & C[i + j], sum1);
            _mm256_storeu_si256((__m256i*) & C[i + j + 8], sum2);
        }

        for (; j <= lenB - 8; j += 8) {
            __m256i b_vec = _mm256_loadu_si256((__m256i*) & B[j]);
            __m256i c_vec = _mm256_loadu_si256((__m256i*) & C[i + j]);

            __m256i product = _mm256_mullo_epi32(a_vec, b_vec);
            __m256i sum = _mm256_add_epi32(c_vec, product);

            _mm256_storeu_si256((__m256i*) & C[i + j], sum);
        }

        for (; j < lenB; j++) {
            C[i + j] += a_val * B[j];
        }
    }

    int carry = 0;
    for (int i = 0; i < lenA + lenB; i++) {
        int total = C[i] + carry;
        carry = total / BASE;
        C[i] = total % BASE;
    }
}

int main() {
    char strA[MAX_DIGITS], strB[MAX_DIGITS];
    int A[MAX_DIGITS], B[MAX_DIGITS];
    int C[2 * MAX_DIGITS];

    memset(A, 0, sizeof(A));
    memset(B, 0, sizeof(B));
    memset(C, 0, sizeof(C));

    printf("Please enter the first number A:\n");
    scanf("%s", strA);
    printf("Please enter the second number B:\n");
    scanf("%s", strB);

    int lenA = stringToBigInt(strA, A);
    int lenB = stringToBigInt(strB, B);

    struct timespec start, end;
    double time_vec;

    printf("\n=== Testing Vectorized Optimized Version ===\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    multiply_vectorized_optimized(A, lenA, B, lenB, C);
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_vec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Vectorized Result = ");
    printBigInt(C, lenA + lenB);
    printf("Vectorized Time: %.8f seconds\n", time_vec);

    return 0;
}