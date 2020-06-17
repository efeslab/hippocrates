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

int main(int argc, char *argv[]) {
    char arr[100];

	void *p = NULL;

	PMTest_init(p, 2);
	PMTest_START;

	PMTest_assign((void *)(&arr[0]), 4);
	PMTest_fence();
	PMTest_assign((void *)(&arr[4]), 4);
	PMTest_flush((void *)(&arr[0]), 4);
	PMTest_flush((void *)(&arr[0]), 4);
	PMTest_isPersistent((void *)(&arr[0]), 4);
	PMTest_isPersistent((void *)(&arr[0]), 0);
	PMTest_isPersistedBefore((void *)(&arr[0]), 4, (void *)(&arr[0]), 4);

	PMTest_sendTrace(p);

	PMTest_END;
	PMTest_getResult(p);
	PMTest_exit(p);

	printf("Test complete!\n");
}
