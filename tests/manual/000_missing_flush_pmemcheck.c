#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <immintrin.h>

#include <valgrind/pmemcheck.h>

/**
 */

void correct(char *arr) {
	*arr = 'c';
	_mm_clwb(arr);
	_mm_sfence();
}

void incorrect(char *arr) {
	*arr = 'i';
	// _mm_clwb(arr);
	_mm_sfence();
}

int main(int argc, char *argv[]) {
	char arr[1024];
	VALGRIND_PMC_REGISTER_PMEM_MAPPING(arr, sizeof(arr));

	printf("Starting testing...\n");

	correct(&arr[0]);
	incorrect(&arr[64]);
	correct(&arr[128]);
	incorrect(&arr[192]);

	printf("Test complete!\n");

	VALGRIND_PMC_REMOVE_PMEM_MAPPING(arr, sizeof (arr));
	
	return 0;
}
