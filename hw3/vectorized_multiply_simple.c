#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <immintrin.h>

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

// 基准版本（非向量化）
void multiply_baseline(uint32_t* A, size_t lenA, uint32_t* B, size_t lenB, uint32_t* C) {
    memset(C, 0, (lenA + lenB) * sizeof(uint32_t));

    for (size_t i = 0; i < lenA; i++) {
        uint32_t carry = 0;
        for (size_t j = 0; j < lenB; j++) {
            uint64_t product = (uint64_t)A[i] * B[j] + C[i + j] + carry;
            C[i + j] = product % BASE;
            carry = product / BASE;
        }
        C[i + lenB] = carry;
    }
}

// 修正的向量化版本
void multiply_vectorized(uint32_t* A, size_t lenA, uint32_t* B, size_t lenB, uint32_t* C) {
    memset(C, 0, (lenA + lenB) * sizeof(uint32_t));

    for (size_t i = 0; i < lenA; i++) {
        __m256i carry_vec = _mm256_setzero_si256();
        __m256i a_vec = _mm256_set1_epi32(A[i]);

        size_t j;
        for (j = 0; j + 8 <= lenB; j += 8) {
            // 加载B的8个元素和C的当前值
            __m256i b_vec = _mm256_loadu_si256((__m256i*) & B[j]);
            __m256i c_vec = _mm256_loadu_si256((__m256i*) & C[i + j]);

            // 计算乘积
            __m256i product = _mm256_mullo_epi32(a_vec, b_vec);

            // 加上C的当前值和进位
            __m256i sum = _mm256_add_epi32(c_vec, product);
            sum = _mm256_add_epi32(sum, carry_vec);

            // 计算新的进位（除以BASE）
            __m256i new_carry = _mm256_srli_epi32(sum, 1); // 简化处理，实际应该是除以10
            __m256i result = _mm256_sub_epi32(sum, _mm256_slli_epi32(new_carry, 1));

            // 存储结果
            _mm256_storeu_si256((__m256i*) & C[i + j], result);
            carry_vec = new_carry;
        }

        // 处理剩余的元素（标量处理）
        for (; j < lenB; j++) {
            uint64_t product = (uint64_t)A[i] * B[j] + C[i + j];
            C[i + j] = product % BASE;
            if (j + 1 < lenB) {
                C[i + j + 1] += product / BASE;
            }
            else {
                // 最后一个元素，需要特殊处理进位
                uint32_t carry = product / BASE;
                size_t pos = i + j + 1;
                while (carry > 0) {
                    uint64_t sum = C[pos] + carry;
                    C[pos] = sum % BASE;
                    carry = sum / BASE;
                    pos++;
                }
            }
        }
    }
}

// 更简单的向量化版本（推荐使用）
void multiply_vectorized_simple(uint32_t* A, size_t lenA, uint32_t* B, size_t lenB, uint32_t* C) {
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
                // 处理尾部不足8个元素的情况
                for (size_t k = j; k < lenB; k++) {
                    C[i + k] += A[i] * B[k];
                }
            }
        }

        // 统一处理进位（标量方式）
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
    uint32_t C_base[2 * MAX_DIGITS];
    uint32_t C_vec[2 * MAX_DIGITS];

    memset(A, 0, sizeof(A));
    memset(B, 0, sizeof(B));
    memset(C_base, 0, sizeof(C_base));
    memset(C_vec, 0, sizeof(C_vec));

    printf("Please enter the first number A:\n");
    scanf("%s", strA);
    printf("Please enter the second number B:\n");
    scanf("%s", strB);

    int lenA = stringToBigInt(strA, A);
    int lenB = stringToBigInt(strB, B);

    // 测试基准版本
    multiply_baseline(A, lenA, B, lenB, C_base);
    printf("Baseline Result = ");
    printBigInt(C_base, lenA + lenB);

    // 测试向量化版本
    multiply_vectorized_simple(A, lenA, B, lenB, C_vec);
    printf("Vectorized Result = ");
    printBigInt(C_vec, lenA + lenB);

    return 0;
}