#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_DIGITS 2048  // max digits for each number 1024
#define BASE 10          // base of each bit 10


void printBigInt(int *num, int len) {
    int i = len - 1;
    while (i > 0 && num[i] == 0) i--;  //remove the high bit 0
    for (; i >= 0; i--)
        printf("%d", num[i]);
    printf("\n");
}

int stringToBigInt(const char *str, int *num) {
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        // str[len-1-i] from last digit
        num[i] = str[len - 1 - i] - '0';
    }
    return len;  // return the length of the number
}

// Multi-precision 
void multiplyBigInt(int *A, int lenA, int *B, int lenB, int *C) {
    // initialization
    memset(C, 0, sizeof(int) * (lenA + lenB));

    // bit by bit multiply
    for (int i = 0; i < lenA; i++) {
        for (int j = 0; j < lenB; j++) {
            C[i + j] += A[i] * B[j];
        }
    }

    // carry
    for (int k = 0; k < lenA + lenB; k++) {
        if (C[k] >= BASE) {
            C[k + 1] += C[k] / BASE;
            C[k] %= BASE;
        }
    }
}

int main() {
    char strA[MAX_DIGITS], strB[MAX_DIGITS];
    int A[MAX_DIGITS], B[MAX_DIGITS], C[2 * MAX_DIGITS];
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
    
    struct timespec start, end;
    double time_spent;

    clock_gettime(CLOCK_MONOTONIC, &start);
    multiplyBigInt(A, lenA, B, lenB, C);
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_spent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Elapsed time in seconds: %.8f \n", time_spent);

    printf("Result = ");
    printBigInt(C, lenA + lenB);
    return 0;
}