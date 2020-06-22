#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <immintrin.h>

#include <sys/time.h>

#include <libpmem.h>

void do_gettimeofday(struct timeval *tv) {
    int ret = gettimeofday(tv, NULL);
    if (ret) {
        perror("time start");
        exit(-1);
    }

    _mm_mfence();
}

uint64_t get_diff(struct timeval *start, struct timeval *end) {
    uint64_t seconds  = end->tv_sec - start->tv_sec;
    uint64_t useconds = end->tv_usec - start->tv_usec;
    return useconds + (seconds * 1000000); 
}

void correct(void *pmem, size_t len, size_t nops) {
    uint8_t *addr_min = (uint8_t*)pmem;
    uint8_t *addr = addr_min;
    uint8_t *addr_max = pmem + len;
    uint8_t val = (uint8_t)rand();

    for (size_t i = 0; i < nops; ++i) {
        *addr = (uint8_t)(val + i);
        _mm_clflushopt(addr);
        _mm_sfence();
        // next
        addr = (addr + 64) >= addr_max ? addr_min : addr + 64;
    }
}

void non_durable(void *pmem, size_t len, size_t nops) {
    uint8_t *addr_min = (uint8_t*)pmem;
    uint8_t *addr = addr_min;
    uint8_t *addr_max = pmem + len;
    uint8_t val = (uint8_t)rand();

    for (size_t i = 0; i < nops; ++i) {
        *addr = (uint8_t)(val + i);
        // next
        addr = (addr + 64) >= addr_max ? addr_min : addr + 64;
    }
}

void perf_bug(void *pmem, size_t len, size_t nops) {
    uint8_t *addr_min = (uint8_t*)pmem;
    uint8_t *addr = addr_min;
    uint8_t *addr_max = pmem + len;
    uint8_t val = (uint8_t)rand();

    for (size_t i = 0; i < nops; ++i) {
        *addr = (uint8_t)(val + i);
        _mm_sfence();
        _mm_clflushopt(addr);
        _mm_sfence();
        // next
        addr = (addr + 64) >= addr_max ? addr_min : addr + 64;
    }
}


uint64_t runner(void (*kernel)(void*,size_t,size_t), void *pmem, size_t len, size_t nops) {
    struct timeval start, end;

    pmem_memset_persist(pmem, 0, len);

    do_gettimeofday(&start);

    kernel(pmem, len, nops);

    do_gettimeofday(&end);

    return get_diff(&start, &end);
}

int main(int argc, char *argv[]) {
	if (argc < 4) {
        fprintf(stderr, "Usage: %s <file> <size> <nops>\n", argv[0]);
        return -1;
    }

    srand(time(NULL));

    void *pmemaddr;

    char *fname = argv[1];
    size_t len = atoll(argv[2]);
    size_t mapped_len;
    size_t nops = atoll(argv[3]);
    int is_pmem;

	if ((pmemaddr = pmem_map_file(fname, len, PMEM_FILE_CREATE, 0666,
                                  &mapped_len, &is_pmem)) == NULL) {
		perror("pmem_map_file");
		exit(1);
	}

    if (!is_pmem) {
        fprintf(stderr, "Error: region is not PMEM! Performance numbers will be inaccurate.\n");
    }   

    if (mapped_len > len) {
        printf("Warning: Original length: %lu, mapped: %lu\n", len, mapped_len);
    }

    uint64_t usec_correct = runner(correct, pmemaddr, mapped_len, nops);
    uint64_t usec_non_durable = runner(non_durable, pmemaddr, mapped_len, nops);
    uint64_t usec_perf_bug = runner(perf_bug, pmemaddr, mapped_len, nops);

    printf("Correct: %lu usec\nNo Persists: %lu usec\nRedundant Persists: %lu usec\n",
            usec_correct, usec_non_durable, usec_perf_bug);

    pmem_unmap(pmemaddr, mapped_len);

    return 0;
}
