#include <stdint.h>
#include <stddef.h>
#include <immintrin.h>

// vectorized multiplication using intrinsics
void multiply_vectorized(uint32_t *a, size_t na, uint32_t *b, size_t nb, uint32_t *result) {
    // Initialize result array to zero
    memset(result, 0, (na + nb) * sizeof(uint32_t));

    for (size_t i = 0; i < na; i++) {
        // broadcast a[i] to all elements of a vector
        __m256i a_vec = _mm256_set1_epi32(a[i]);
        uint64_t carry = 0;

        for (size_t j = 0; j < nb; j += 8) {
            // load 8 elements from b and result
            __m256i b_vec = _mm256_loadu_si256((__m256i*)&b[j]);
            __m256i res_vec = _mm256_loadu_si256((__m256i*)&result[i + j]);

            // perform multiplication in 8 parallel lanes
            __m256i prod_low = _mm256_mullo_epi32(a_vec, b_vec);
            __m256i prod_high = _mm256_mulhi_epu32(a_vec, b_vec);

            // add carry to the lower part
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