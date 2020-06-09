#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <libpmem.h>
#include <pmtest.h>

int main(int argc, char *argv[]) {
    int fd = open("/tmp/000_simple_bug.pmem", O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
      perror("open");
    }
}
