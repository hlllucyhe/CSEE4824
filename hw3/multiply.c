#include <stdint.h>
#include <stddef.h>
#include <string.h>

void multiply(uint32_t *a, size_t na, uint32_t *b, size_t nb, uint32_t *result) {
    // Initialize result array to zero
    memset(result, 0, (na + nb) * sizeof(uint32_t));

    // Perform multiplication using the grade-school algorithm
    for (size_t i = 0; i < na; i++) {
        uint64_t carry = 0;
        for (size_t j = 0; j < nb; j++) {
            uint64_t prod = (uint64_t)a[i] * (uint64_t)b[j] + (uint64_t)result[i + j] + carry;
            result[i + j] = (uint32_t)(prod & 0xFFFFFFFF);
            carry = prod >> 32;
        }
        result[i + nb] += (uint32_t)carry;
    }
}

//vectorized version using intrinsics
#ifdef __AVX2__
#include <immintrin.h>
void multiply_vectorized(uint32_t *a, size_t na, uint32_t *b, size_t nb, uint32_t *result) {
    // Initialize result array to zero
    memset(result, 0, (na + nb) * sizeof(uint32_t));

    for (size_t i = 0; i < na; i++) {
        __m256i a_vec = _mm256_set1_epi32(a[i]);
        uint64_t carry = 0;

        for (size_t j = 0; j < nb; j += 8) {
            __m256i b_vec = _mm256_loadu_si256((__m256i*)&b[j]);
            __m256i res_vec = _mm256_loadu_si256((__m256i*)&result[i + j]);

            __m256i prod_low = _mm256_mullo_epi32(a_vec, b_vec);
            __m256i prod_high = _mm256_mulhi_epu32(a_vec, b_vec);

            __m256i carry_vec = _mm256_set1_epi32((uint32_t)carry);
            __m256i sum_low = _mm256_add_epi32(res_vec, prod_low);
            __m256i sum_with_carry = _mm256_add_epi32(sum_low, carry_vec);

            // Store the lower part of the result
            _mm256_storeu_si256((__m256i*)&result[i + j], sum_with_carry);

            // Calculate new carry
            __m256i overflow_mask = _mm256_cmpgt_epi32(sum_with_carry, sum_low);
            carry = _mm256_movemask_epi8(overflow_mask) ? 1 : 0;

            // Add high part to carry
            carry += _mm256_extract_epi32(prod_high, 0);
        }

        result[i + nb] += (uint32_t)carry;
    }
}
#else
void multiply_vectorized(uint32_t *a, size_t na, uint32_t *b, size_t nb, uint32_t *result) {
    // Fallback to the non-vectorized implementation
    multiply(a, na, b, nb, result);
}
#endif