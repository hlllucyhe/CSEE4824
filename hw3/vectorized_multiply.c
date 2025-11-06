#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <immintrin.h>
#include <time.h>

#define MAX_DIGITS 2048
#define BASE 10

int stringToBigInt(const char* str, uint32_t* num) {
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        num[i] = str[len - 1 - i] - '0';
    }
    return len;
}

void printBigInt(uint32_t* num, int len) {
    int i = len - 1;
    while (i > 0 && num[i] == 0) i--;
    for (; i >= 0; i--)
        printf("%u", num[i]);
    printf("\n");
}

void multiply_vectorized(uint32_t* A, size_t lenA, uint32_t* B, size_t lenB, uint32_t* C) {
    memset(C, 0, (lenA + lenB) * sizeof(uint32_t));

    for (size_t i = 0; i < lenA; i++) {
        __m256i a_vec = _mm256_set1_epi32(A[i]);

        for (size_t j = 0; j < lenB; j += 8) {
            if (j + 8 <= lenB) {
                __m256i b_vec = _mm256_loadu_si256((__m256i*) & B[j]);
                __m256i c_vec = _mm256_loadu_si256((__m256i*) & C[i + j]);

                __m256i product = _mm256_mullo_epi32(a_vec, b_vec);
                __m256i sum = _mm256_add_epi32(c_vec, product);

                _mm256_storeu_si256((__m256i*) & C[i + j], sum);
            }
            else {
                for (size_t k = j; k < lenB; k++) {
                    C[i + k] += A[i] * B[k];
                }
            }
        }

        for (size_t k = 0; k < lenA + lenB - 1; k++) {
            if (C[k] >= BASE) {
                C[k + 1] += C[k] / BASE;
                C[k] %= BASE;
            }
        }
    }
}

int main() {
    char strA[MAX_DIGITS], strB[MAX_DIGITS];
    uint32_t A[MAX_DIGITS], B[MAX_DIGITS];
    uint32_t C[2 * MAX_DIGITS];

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
    double time_vectorized;

    clock_gettime(CLOCK_MONOTONIC, &start);
    multiply_vectorized(A, lenA, B, lenB, C);
    clock_gettime(CLOCK_MONOTONIC, &end);

    time_vectorized = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Vectorized Result = ");
    printBigInt(C, lenA + lenB);
    printf("Vectorized Time: %.8f seconds\n", time_vectorized);

    return 0;
}