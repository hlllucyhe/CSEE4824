#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>  
#include <math.h>    

// random data generator for integer arrays
void generate_data(const char* filename, size_t size) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    srand((unsigned)time(NULL));
    for (size_t i = 0; i < size; i++) {
        int num = rand();
        fwrite(&num, sizeof(int), 1, file);
    }
    fclose(file);
}

// generate sorted data generator for integer arrays
void generate_sorted_data(const char* filename, size_t size) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < size; i++) {
        int num = (int)i; // sorted data
        fwrite(&num, sizeof(int), 1, file);
    }
    fclose(file);
}

//generate reverse sorted data generator for integer arrays
void generate_reverse_sorted_data(const char* filename, size_t size) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < size; i++) {
        int num = (int)(size - i); // reverse sorted data
        fwrite(&num, sizeof(int), 1, file);
    }
    fclose(file);
}

//generate normal data generator for integer arrays
void generate_normal_data(const char* filename, size_t size) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    srand((unsigned)time(NULL));
    for (size_t i = 0; i < size; i++) {
        // generate numbers with normal distribution
        double u1 = ((double)rand() / RAND_MAX);
        double u2 = ((double)rand() / RAND_MAX);
        double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979323846 * u2);
        int num = (int)(z0 * 1000 + 5000); // mean=5000, stddev=1000
        fwrite(&num, sizeof(int), 1, file);
    }
    fclose(file);
}

// generate datasets from 10^4 to 10^9 integer arrays to test merge sort
int main() {
    size_t sizes[] = {10000, 1000000, 1000000000};
    const char* patterns[] = {"random", "sorted", "reverse_sorted", "normal"};
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        for (size_t j = 0; j < sizeof(patterns)/sizeof(patterns[0]); j++) {
            char filename[256];
            snprintf(filename, sizeof(filename), "data_%s_%zu.bin", patterns[j], sizes[i]);
            if (strcmp(patterns[j], "random") == 0) {
                generate_data(filename, sizes[i]);
            } else if (strcmp(patterns[j], "sorted") == 0) {
                generate_sorted_data(filename, sizes[i]);
            } else if (strcmp(patterns[j], "reverse_sorted") == 0) {
                generate_reverse_sorted_data(filename, sizes[i]);
            } else if (strcmp(patterns[j], "normal") == 0) {
                generate_normal_data(filename, sizes[i]);
            }
            printf("Generated %s data of size %zu in file %s\n", patterns[j], sizes[i], filename);
        }
    }
    return 0;
}