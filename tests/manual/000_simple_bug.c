#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <pmtest.h>

/**
 * Adapted from PMTest test #1.
 * https://github.com/sihangliu/pmtest/blob/master/src/test.cc
 */

void correct(void *p) {
	char arr[100];
	PMTest_assign((void *)(&arr[0]), 4);
	PMTest_flush((void *)(&arr[0]), 4);
	PMTest_fence();

	PMTest_assign((void *)(&arr[4]), 4);
	PMTest_flush((void *)(&arr[4]), 4);
	PMTest_fence();

	PMTest_isPersistent((void *)(&arr[0]), 0);
	PMTest_isPersistent((void *)(&arr[4]), 4);
	PMTest_isPersistedBefore((void *)(&arr[0]), 4, (void *)(&arr[4]), 4);
	PMTest_sendTrace(p);
}

void incorrect(void *p) {
	char arr[100];
	PMTest_assign((void *)(&arr[0]), 4);
	PMTest_flush((void *)(&arr[0]), 4);
	PMTest_fence();

	PMTest_assign((void *)(&arr[4]), 4);
	// PMTest_flush((void *)(&arr[4]), 4);
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

	correct(p);
	incorrect(p);

	PMTest_END;
	PMTest_getResult(p);
	PMTest_exit(p);

	printf("Test complete!\n");
}
