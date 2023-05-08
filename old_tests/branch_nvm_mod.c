#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <libpmem.h>
#include <stdbool.h>

#define BUF_LEN 4096

int mod_function(char *addr, bool mod) {
    char a = 2;
    if (mod) {
        addr[0] = a;
    } else {
        a = addr[0];
    }

    return a;
}

int loop_function(char *addr, int count) {
    for (int i = 0; i < count; ++i) {
        addr[i] = (char)i;
    }

    return 0;
}

int loop_extra(char *addr, int count) {
    for (int i = 0; i < count; ++i) {
        addr[i] = (char)i;
    }

    addr[0] = 'S';

    return 0;
}

int main(int argc, char *argv[]) {
	char __attribute__((annotate("nvmptr"))) *pmemaddr;

    char data[BUF_LEN];

    pmemaddr = data;

    mod_function(pmemaddr, true);
    mod_function(pmemaddr, false);

    loop_function(pmemaddr, 10);
    loop_extra(pmemaddr, 10);

	return 0;
}
