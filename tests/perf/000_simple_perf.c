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

void extra_flush(void *pmem, size_t len, size_t nops, double ratio) {
    uint8_t *addr_min = (uint8_t*)pmem;
    uint8_t *addr = addr_min;
    uint8_t *addr_max = pmem + len;
    uint8_t val = (uint8_t)rand();

    double count = ratio;

    for (size_t i = 0; i < nops; ++i) {
        *addr = (uint8_t)(val + i);
        _mm_clflushopt(addr);
        if (count > 1.0) {
            _mm_clflushopt(addr);
            count -= 1.0;
        }
        _mm_sfence();
        // next
        addr = (addr + 64) >= addr_max ? addr_min : addr + 64;
        count += ratio;
    }
}

void extra_fence(void *pmem, size_t len, size_t nops, double ratio) {
    uint8_t *addr_min = (uint8_t*)pmem;
    uint8_t *addr = addr_min;
    uint8_t *addr_max = pmem + len;
    uint8_t val = (uint8_t)rand();

    double count = ratio;

    for (size_t i = 0; i < nops; ++i) {
        if (count > 1.0) {
            _mm_sfence();
            count -= 1.0;
        }
        *addr = (uint8_t)(val + i);
        _mm_clwb(addr);
        _mm_sfence();
        // next
        addr = (addr + 64) >= addr_max ? addr_min : addr + 64;
        count += ratio;
    }
}

uint64_t runner(void (*kernel)(void*,size_t,size_t), void *pmem, size_t len, size_t nops) {
    struct timeval start, end;

    pmem_memset_persist(pmem, 0, len);

    do_gettimeofday(&start);

    _mm_mfence();
    kernel(pmem, len, nops);
    _mm_mfence();

    do_gettimeofday(&end);

    return get_diff(&start, &end);
}


uint64_t config_runner(void (*kernel)(void*,size_t,size_t,double), 
                       void *pmem, size_t len, size_t nops, double ratio) {
    struct timeval start, end;

    pmem_memset_persist(pmem, 0, len);

    do_gettimeofday(&start);

    _mm_mfence();
    kernel(pmem, len, nops, ratio);
    _mm_mfence();

    do_gettimeofday(&end);

    return get_diff(&start, &end);
}

int main(int argc, char *argv[]) {
	if (argc < 5) {
        fprintf(stderr, "Usage: %s <file> <size> <nops> <trials>\n", argv[0]);
        return -1;
    }

    srand(time(NULL));

    void *pmemaddr;

    char *fname = argv[1];
    size_t len = atoll(argv[2]);
    size_t mapped_len;
    size_t nops = atoll(argv[3]);
    size_t ntrials = atoll(argv[4]);
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

    // Do trials, output to CSV to stdout.

    printf("File Size,Num Ops,Num Trials,Trial Num,"
           "Correct Total Time (usec),Correct Time/Op (usec),"
           "Non-Durable Total Time (usec),Non-Durable Time/Op (usec)," 
           "2X Flushes Total Time (usec),2X Flushes Time/Op (usec),"
           "1.5X Flushes Total Time (usec),1.5X Flushes Time/Op (usec),"
           "1.1X Flushes Total Time (usec),1.1X Flushes Time/Op (usec),"
           "2X Fences Total Time (usec),2X Fences Time/Op (usec),"
           "1.5X Fences Total Time (usec),1.5X Fences Time/Op (usec),"
           "1.1X Fences Total Time (usec),1.1X Fences Time/Op (usec)\n");

    for (size_t t = 0; t < ntrials; ++t) {
        printf("%lu,%lu,%lu,%lu,", mapped_len, nops, ntrials, t);
        uint64_t usec;
        double per_op;

#if 1
        usec = runner(correct, pmemaddr, mapped_len, nops);
        per_op = (double)usec / (double)nops;
        printf("%lu,%f,", usec, per_op);
#else
        usec = config_runner(extra_fence, pmemaddr, mapped_len, nops, 1.0);
        per_op = (double)usec / (double)nops;
        printf("%lu,%f,", usec, per_op);
#endif
        usec = runner(non_durable, pmemaddr, mapped_len, nops);
        per_op = (double)usec / (double)nops;
        printf("%lu,%f,", usec, per_op);

        usec = config_runner(extra_flush, pmemaddr, mapped_len, nops, 1.0);
        per_op = (double)usec / (double)nops;
        printf("%lu,%f,", usec, per_op);

        usec = config_runner(extra_flush, pmemaddr, mapped_len, nops, 0.5);
        per_op = (double)usec / (double)nops;
        printf("%lu,%f,", usec, per_op);

        usec = config_runner(extra_flush, pmemaddr, mapped_len, nops, 0.1);
        per_op = (double)usec / (double)nops;
        printf("%lu,%f,", usec, per_op);

        usec = config_runner(extra_fence, pmemaddr, mapped_len, nops, 1.0);
        per_op = (double)usec / (double)nops;
        printf("%lu,%f,", usec, per_op);

        usec = config_runner(extra_fence, pmemaddr, mapped_len, nops, 0.5);
        per_op = (double)usec / (double)nops;
        printf("%lu,%f,", usec, per_op);

        usec = config_runner(extra_fence, pmemaddr, mapped_len, nops, 0.1);
        per_op = (double)usec / (double)nops;
        printf("%lu,%f\n", usec, per_op);
    }


    pmem_unmap(pmemaddr, mapped_len);

    return 0;
}
