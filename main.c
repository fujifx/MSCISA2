/* Compile: gcc main.c libmalloc.c -o A2
 * Run one test at a time: ./A2 clean|double|uaf|overflow|underflow
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void ItSS_HeapTest(void);

int main(int argc, char **argv) {
    char *buffer;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s clean|double|uaf|overflow|underflow\n", argv[0]);
        return 1;
    }

    buffer = malloc(16);
    if (buffer == NULL) {
        fprintf(stderr, "Allocation failed\n");
        return 1;
    }

    if (strcmp(argv[1], "clean") == 0) {
        buffer[0] = 'A';
        free(buffer);
        printf("Freed block remains clean; checking heap.\n");
        ItSS_HeapTest();
    } else if (strcmp(argv[1], "double") == 0) {
        free(buffer);
        printf("Attempting second free; program should abort.\n");
        fflush(stdout);
        free(buffer);
    } else if (strcmp(argv[1], "uaf") == 0) {
        free(buffer);
        buffer[0] = 'X';
        printf("Checking heap after writing to freed memory; should abort.\n");
        fflush(stdout);
        ItSS_HeapTest();
    } else if (strcmp(argv[1], "overflow") == 0) {
        buffer[16] = 'X';
        printf("Checking heap after writing one byte past the buffer; should abort.\n");
        fflush(stdout);
        ItSS_HeapTest();
    } else if (strcmp(argv[1], "underflow") == 0) {
        buffer[-1] = 'X';
        printf("Checking heap after writing one byte before the buffer; should abort.\n");
        fflush(stdout);
        ItSS_HeapTest();
    } else {
        fprintf(stderr, "Unknown test: %s\n", argv[1]);
        return 1;
    }

    printf("Heap verification completed.\n");
    return 0;
}
