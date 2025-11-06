#include <stdint.h>
#include <stddef.h>
#include <immintrin.h>

#define MAX_DIGITS 2048  // max digits for each number 1024
#define BASE 10          // base of each bit 10

int stringToBigInt(const char *str, uint32_t *num) {
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        num[i] = str[len - 1 - i] - '0';
    }
    return len;
}

void printBigInt(uint64_t *num, int len) {
    int i = len - 1;
    while (i > 0 && num[i] == 0) i--; 
    for (; i >= 0; i--)
        printf("%llu", (unsigned long long)num[i]);
    printf("\n");
}

// vectorized multiplication using intrinsics
void multiply_vectorized(uint32_t *A, size_t lenA, uint32_t *B, size_t lenB, uint32_t *C) {
    // Initialize result array to zero
    memset(C, 0, (lenA + lenB) * sizeof(uint32_t));

    for (size_t i = 0; i < lenA; i++) {
        // broadcast a[i] to all elements of a vector
        __m256i A_vec = _mm256_set1_epi32(A[i]);
        uint64_t carry = 0;

        for (size_t j = 0; j < lenB; j += 8) {
            // load 8 elements from b and result
            __m256i B_vec = _mm256_loadu_si256((__m256i*)&B[j]);
            __m256i C_vec = _mm256_loadu_si256((__m256i*)&C[i + j]);

            // perform multiplication in 8 parallel lanes
            __m256i prod_low = _mm256_mullo_epi32(A_vec, B_vec);
            __m256i prod_even = _mm256_mul_epu32(A_vec, B_vec);
            __m256i prod_high = _mm256_srli_epi64(prod_even, 32);

            // add carry to the lower part
            __m256i carry_vec = _mm256_set1_epi32((uint32_t)carry);
            __m256i sum_low = _mm256_add_epi32(C_vec, prod_low);
            __m256i sum_with_carry = _mm256_add_epi32(sum_low, carry_vec);

            // Store the lower part of the result
            _mm256_storeu_si256((__m256i*)&C[i + j], sum_with_carry);

            // Calculate new carry
            __m256i overflow_mask = _mm256_cmpgt_epi32(sum_with_carry, sum_low);
            carry = _mm256_movemask_epi8(overflow_mask) ? 1 : 0;

            // Add high part to carry
            carry += _mm256_extract_epi32(prod_high, 0);
        }

        C[i + lenB] += (uint32_t)carry;
    }
}
int main() {
    char strA[MAX_DIGITS], strB[MAX_DIGITS];
    uint32_t A[MAX_DIGITS], B[MAX_DIGITS];
    uint64_t C[2 * MAX_DIGITS];
    memset(A, 0, sizeof(A));
    memset(B, 0, sizeof(B));
    memset(C, 0, sizeof(C));

    printf("Please enter the first number A:\n");
    scanf("%s",strA);

    printf("Please enter the first number B:\n");
    scanf("%s",strB);

    //convert string to arrays
    int lenA = stringToBigInt(strA, A);
    int lenB = stringToBigInt(strB, B);

    multiply_vectorized(A, lenA, B, lenB, C);

    printf("Result = ");
    printBigInt(C, lenA + lenB);
    return 0;
}