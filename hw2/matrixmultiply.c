#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define n 64 //dimension of square matrices.

void printMat(int matA[n][n]);
void multMat(int matA[n][n], int matB[n][n]);

int main()
{
    time_t t;
    srand((unsigned) time(&t));
    int mat1[n][n];
    int mat2[n][n];
    
    for (int i = 0; i < n; i++){
            for (int j = 0; j < n; j++){
                    mat1[i][j] = rand() % 30;
            }
    }
    
    for (int i = 0; i < n; i++){
            for (int j = 0; j < n; j++){
                    mat2[i][j] = rand() % 30;
            }
    }
    
    //printMat(mat1);
    //printMat(mat2);
    struct timespec start, end;
    double time_spent;

    //matrix multiplication
    clock_gettime(CLOCK_REALTIME, &start);
    multMat(mat1, mat2);
    clock_gettime(CLOCK_REALTIME, &end);
    time_spent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    printf("Elapsed time in seconds: %f \n", time_spent);

    return 0;
}

void multMat(int matA[n][n], int matB[n][n]){
        int result[n][n];
        
        for (int i = 0; i < n; i++){
                for (int j = 0; j < n; j++){
                        result[i][j] = 0;
                        for (int k = 0; k < n; k++){
                                result[i][j] += matA[i][k] * matB[k][j];
                        }
                        
                        //printf("%d\t", result[i][j]);
                }
                
                //printf("\n");
                
        }
}

void printMat(int matA[n][n]){
        for (int i = 0; i < 4; i++){
                for (int j = 0; j < 4; j++){
                    printf("%d\t", matA[i][j]);
            }
        printf("\n");
    }
        printf("\n");
}

