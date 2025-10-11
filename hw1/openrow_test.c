#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <x86intrin.h>   // _mm_clflush, _mm_lfence, _mm_mfence, __rdtscp

// --------- Tunables (keep small & simple) ----------
#define ARENA_MB   256          // arena size to sample addresses from
#define CACHELINE  64
#define PAGE       4096
#define WARMUP     10
#define TRIALS     200          // per A->B->A timing
#define SCAN_STRIDE (64*1024)   // step when scanning B candidates (64 KiB)

// ---------- Helpers you already used ----------
static inline void clflush_range(void *p, size_t len) {
    uintptr_t a = (uintptr_t)p & ~(uintptr_t)(CACHELINE - 1);
    uintptr_t e = (uintptr_t)p + len;
    for (; a < e; a += CACHELINE) _mm_clflush((void*)a);
    _mm_mfence(); // ensure flush completes
}

static inline uint64_t tsc_now(void) {
    unsigned aux;
    _mm_lfence();
    uint64_t t = __rdtscp(&aux);
    _mm_lfence();
    return t;
}

static inline void prefault_touch(char *buf, size_t bytes) {
    for (size_t i = 0; i < bytes; i += PAGE) buf[i] = (char)(i);
    if (bytes) buf[bytes-1] ^= 0; // touch tail
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
    return (x > y) - (x < y);
}

// Measure Δ for A->B->A (loads) with flushes to force DRAM behavior.
static uint64_t time_ABA(char *A, char *B) {
    uint64_t t[TRIALS];

    // Warmup
    for (int i = 0; i < WARMUP; ++i) {
        clflush_range(A, CACHELINE);
        clflush_range(B, CACHELINE);
        (void)*(volatile char*)A;
        (void)*(volatile char*)B;
        (void)*(volatile char*)A;
    }

    for (int r = 0; r < TRIALS; ++r) {
        clflush_range(A, CACHELINE);
        clflush_range(B, CACHELINE);
        uint64_t t0 = tsc_now();
        // Three dependent-ish loads (prevent reordering via volatile)
        volatile char x;
        x = *A;
        x = *B;
        x = *A;
        (void)x;
        uint64_t t1 = tsc_now();
        t[r] = t1 - t0;
    }
    qsort(t, TRIALS, sizeof(uint64_t), cmp_u64);
    return t[TRIALS/2]; // median
}

int main(void) {
    size_t arena_bytes = (size_t)ARENA_MB << 20;
    // 64B-aligned arena
    char *arena = NULL;
    if (posix_memalign((void**)&arena, CACHELINE, arena_bytes)) {
        perror("posix_memalign"); return 1;
    }
    memset(arena, 0xA5, arena_bytes);
    prefault_touch(arena, arena_bytes);

    // Pick a base A (start of arena is fine for this minimal probe)
    char *A = arena;

    // Scan candidate B addresses to find min and max median Δ
    uint64_t best_min = UINT64_MAX, best_max = 0;
    char *B_min = NULL, *B_max = NULL;

    for (size_t off = SCAN_STRIDE; off + CACHELINE < arena_bytes; off += SCAN_STRIDE) {
        char *B = arena + off;
        uint64_t med = time_ABA(A, B);
        if (med < best_min) { best_min = med; B_min = B; }
        if (med > best_max) { best_max = med; B_max = B; }
    }

    // Choose a same-row candidate C as a small offset from A.
    // (Typical DDR4 row buffer is on the order of KBs; small column offsets
    //  like 256–1024B tend to stay within the same row.)
    char *C = A + 512; // small column offset guess

    uint64_t med_hit = time_ABA(A, C);       // likely row-hit
    uint64_t med_noconf = time_ABA(A, B_min); // different bank or benign mapping
    uint64_t med_conf = time_ABA(A, B_max);   // likely same-bank different-row (row conflict)

    printf("Results (median ticks):\n");
    printf("  Row-hit      (A->C->A, C=A+512):         %" PRIu64 "\n", med_hit);
    printf("  No-conflict  (A->Bmin->A):               %" PRIu64 "\n", best_min);
    printf("  Row-conflict (A->Bmax->A):               %" PRIu64 "\n", best_max);

    printf("\nInterpretation:\n");
    if (med_conf > med_hit * 1.5 && med_conf > med_noconf * 1.5) {
        printf("  Row-conflict ≫ Row-hit/No-conflict ⇒ Controller behaves OPEN-ROW.\n");
    } else if ( (llabs((long long)med_hit - (long long)med_noconf) < (long long)(0.1*med_hit)) &&
                (llabs((long long)med_conf - (long long)med_hit) < (long long)(0.2*med_hit)) ) {
        printf("  All similar ⇒ Controller behaves CLOSED-ROW (or aggressively close-page).\n");
    } else {
        printf("  Mixed deltas ⇒ Behavior might be adaptive/hybrid; try different A or strides.\n");
    }

    free(arena);
    return 0;
}

