/**
 * Malloc
 * CS 241 - Fall 2019
 */
#include "tester-utils.h"

#define TOTAL_ALLOCS 5 * M

int main() {
    malloc(1);

    int i;
    int **arr = malloc(TOTAL_ALLOCS * sizeof(int *));
    if (arr == NULL) {
        fprintf(stderr, "Memory failed to allocate!\n");
        return 1;
    }

    for (i = 0; i < TOTAL_ALLOCS; i++) {
        arr[i] = malloc(sizeof(int));
        if (arr[i] == NULL) {
            fprintf(stderr, "Memory failed to allocate!\n");
            return 1;
        }
        //fprintf(stderr, "i = %d, total allocs is %ld\n", i, TOTAL_ALLOCS);
        *(arr[i]) = i;
    }
    //fprintf(stderr, "sha bi\n");
    for (i = 0; i < TOTAL_ALLOCS; i++) {
        if (*(arr[i]) != i) {
            fprintf(stderr, "Memory failed to contain correct data after many "
                            "allocations!\n");
            return 2;
        }
    }

    for (i = 0; i < TOTAL_ALLOCS; i++)
        free(arr[i]);

    free(arr);
    fprintf(stderr, "Memory was allocated, used, and freed!\n");
    return 0;
}
