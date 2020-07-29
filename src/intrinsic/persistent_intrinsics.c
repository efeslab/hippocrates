#include <immintrin.h>
#include <stdint.h>
#include <stdbool.h>

#include <valgrind/pmemcheck.h>

#define PMFIXER(name) PMFIXER_ ## name

void PMFIXER(store_nt)(void) {
    int i;
    __builtin_nontemporal_store(0, &i);
}

void PMFIXER(memset)(int8_t *s, int8_t c, size_t n, bool _unused) {
    for (size_t i = 0; i < n; ++i) {
        int *ptr = (int*)(s + i);
        int val = (int)c;
        _mm_stream_si32(ptr, val);
        VALGRIND_PMC_DO_FLUSH(ptr, sizeof(*ptr));
    }

    _mm_sfence();
}

void PMFIXER(memcpy)(int8_t *d, int8_t *s, size_t n, bool _unused) {
    for (size_t i = 0; i < n; ++i) {
        int *ptr = (int*)(d + i);
        int val = (int)(s[i]);
        _mm_stream_si32(ptr, val);
        VALGRIND_PMC_DO_FLUSH(ptr, sizeof(*ptr));
    }

    _mm_sfence();
} 