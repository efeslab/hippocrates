#include <immintrin.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <valgrind/pmemcheck.h>

#define PMFIXER(name) PMFIXER_ ## name

void PMFIXER(store_nt)(void) {
    int i;
    __builtin_nontemporal_store(0, &i);
}

/**
 * Memory functions.
 */

void PMFIXER(memset)(uint8_t *s, uint8_t c, size_t n, bool _unused) {
    for (size_t i = 0; i < n; ++i) {
        int *ptr = (int*)(s + i);
        int val = (int)c;
        _mm_stream_si32(ptr, val);
        VALGRIND_PMC_DO_FLUSH(ptr, sizeof(*ptr));
    }

    _mm_sfence();
}

void PMFIXER(memcpy)(uint8_t *d, uint8_t *s, size_t n, bool _unused) {
    for (size_t i = 0; i < n; ++i) {
        int *ptr = (int*)(d + i);
        int val = (int)(s[i]);
        _mm_stream_si32(ptr, val);
        VALGRIND_PMC_DO_FLUSH(ptr, sizeof(*ptr));
    }

    _mm_sfence();
}

void PMFIXER(memmove)(uint8_t *d, uint8_t *s, size_t n, bool _unused) {
    // https://opensource.apple.com/source/network_cmds/network_cmds-481.20.1/unbound/compat/memmove.c.auto.html
    uint8_t* from = (uint8_t*) s;
	uint8_t* to = (uint8_t*) d;

	if (from == to || n == 0) {
        return;
    }

	if (to > from && to-from < n) {
		/* to overlaps with from */
		/*  <from......>         */
		/*         <to........>  */
		/* copy in reverse, to avoid overwriting from */
		for(size_t i = n-1; i >= 0; i--) {
            to[i] = from[i];
        }
        return;
	}

	if (from > to && from-to < (int)n) {
		/* to overlaps with from */
		/*        <from......>   */
		/*  <to........>         */
		/* copy forwards, to avoid overwriting from */
		for (size_t i=0; i < n; i++) {
            to[i] = from[i];
        }
        return;
	}

	PMFIXER(memcpy)(d, s, n, _unused);
}

/**
 * String functions.
 */

char *PMFIXER(strncpy)(char *dest, const char *src, size_t n) {
    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++) {
        // dest[i] = src[i];
        int *ptr = (int*)(dest + i);
        int val = (int)(src[i]);
        _mm_stream_si32(ptr, val);
        VALGRIND_PMC_DO_FLUSH(ptr, sizeof(*ptr));
    }     
    for ( ; i < n; i++) {
        // dest[i] = '\0';
        int *ptr = (int*)(dest + i);
        int val = (int)('\0');
        _mm_stream_si32(ptr, val);
        VALGRIND_PMC_DO_FLUSH(ptr, sizeof(*ptr));
    }  

    _mm_sfence();
    
    return dest;
}