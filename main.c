/* compile line: $ gcc main.c libmalloc.c -o A2 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern void ItSS_HeapTest();

int main() {
	/* test area */
	

    /* done */
	printf("\nVERIFYING HEAP\n");
    ItSS_HeapTest();
    printf("\nDONE\n");
	return 0;
}