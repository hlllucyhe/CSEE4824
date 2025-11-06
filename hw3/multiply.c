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

