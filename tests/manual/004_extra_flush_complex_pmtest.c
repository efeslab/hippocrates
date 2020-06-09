#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <immintrin.h>

#include <pmtest.h>

/**
 * Adapted from PMTest test #1.
 * https://github.com/sihangliu/pmtest/blob/master/src/test.cc
 * 
 * The macros don't actually do anything---they just generate the traces.
 * So, I've augmented the test with calls to persistence mechanisms.
 * 
 * The idea of this test is that PMTest reports an extra flush, but statically 
 * it isn't ALWAYS an extra flush. So, we need to condition our fix based on the
 * branch.
 * 
 */

void incorrect(void *p, bool extra) {
	char arr[100];
	*(int*)(&arr[0]) = 7;
	PMTest_assign((void *)(&arr[0]), 4);

	_mm_clwb(&arr[0]);
	PMTest_flush((void *)(&arr[0]), 4);

	_mm_sfence();
	PMTest_fence();

	*(int*)(&arr[4]) = 7;
	PMTest_assign((void *)(&arr[4]), 4);

	_mm_clwb(&arr[4]);
	PMTest_flush((void *)(&arr[4]), 4);

	if (extra) {
		*(int*)(&arr[4]) = 8;
		PMTest_assign((void *)(&arr[4]), 4);
	}

	// begin extra
	_mm_clwb(&arr[4]);
	PMTest_flush((void *)(&arr[4]), 4);
	// end extra

	_mm_sfence();
	PMTest_fence();

	PMTest_isPersistent((void *)(&arr[0]), 0);
	PMTest_isPersistent((void *)(&arr[4]), 4);
	PMTest_isPersistedBefore((void *)(&arr[0]), 4, (void *)(&arr[4]), 4);
	PMTest_sendTrace(p);
}

int main(int argc, char *argv[]) {
	void *p = NULL;

	printf("Starting testing...\n");

	PMTest_init(p, 2);
	PMTest_START;

	incorrect(p, false);
	incorrect(p, true);

	PMTest_END;
	PMTest_getResult(p);
	PMTest_exit(p);

	printf("Test complete!\n");
}
