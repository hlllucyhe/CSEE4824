#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <x86intrin.h>   // for _mm_clflush, __rdtscp (or use asm)

#ifndef REPEAT
#define REPEAT 1000      // 大尺寸时别用 1e6，会被flush拖垮
#endif
#define WARMUP 10
#define CACHELINE 64
#define PAGE 4096

static inline void clflush_range(void *p, size_t len) {
    uintptr_t addr = (uintptr_t)p;
    uintptr_t end  = addr + len;
    for (; addr < end; addr += CACHELINE) {
        _mm_clflush((void*)addr);
    }
    _mm_mfence();  // 确保flush完成
}

static inline uint64_t tsc_begin(void) {
    unsigned int aux;
    _mm_lfence();                // 序列化
    return __rdtscp(&aux);       // 读tsc并序列化
}

static inline uint64_t tsc_end(void) {
    unsigned int aux;
    uint64_t t = __rdtscp(&aux); // 读tsc并序列化
    _mm_lfence();
    return t;
}

static inline void prefault_touch(char *buf, size_t bytes) {
    // 页预触，避免首次缺页/清零被算进计时
    for (size_t i = 0; i < bytes; i += PAGE) buf[i] = 1;
    if (bytes > 0) buf[bytes-1] ^= 0; // 触到最后一个字节，防越界
}

static inline void memtest(size_t bytes, FILE *out) {
    // 64B 对齐分配（避免跨行边界的无谓抖动）
    char *src, *dst;
    if (posix_memalign((void**)&src, CACHELINE, bytes) ||
        posix_memalign((void**)&dst, CACHELINE, bytes)) {
        perror("posix_memalign"); exit(1);
    }

    // 初始化 & 预触页
    memset(src, 0xA5, bytes);
    memset(dst, 0,    bytes);
    prefault_touch(src, bytes);
    prefault_touch(dst, bytes);

    // 预热：建立i-cache路径/页表/TLB等，不记录
    for (int r = 0; r < WARMUP; ++r) {
        clflush_range(src, bytes);
        clflush_range(dst, bytes);
        (void)memcpy(dst, src, bytes);
    }

    // 正式测量
    for (int r = 0; r < REPEAT; ++r) {
        // 为当前迭代制造“冷”条件：把本次会触达的行都flush
        clflush_range(src, bytes);
        clflush_range(dst, bytes);

        uint64_t t0 = tsc_begin();
        memcpy(dst, src, bytes);
        uint64_t t1 = tsc_end();

        // 防止编译器把 memcpy 优化掉
        // （观察一个字节，使其对外可见）
        asm volatile("" :: "r"(dst[0]) : "memory");

        // 仅记录CSV，避免stdout抖动
        fprintf(out, "%zu,%" PRIu64 "\n", bytes, (t1 - t0));
    }

    free(src);
    free(dst);
}

int main(void) {
    FILE *out = fopen("results.csv", "w");
    if (!out) { perror("fopen"); return 1; }
    fprintf(out, "Size(Bytes),Time(Ticks)\n");

    const int exps[] = {6,7,8,9,10,11,12,13,14,15,16,20,21};
    for (size_t i = 0; i < sizeof(exps)/sizeof(exps[0]); ++i) {
        size_t bytes = (size_t)1 << exps[i];
        memtest(bytes, out);
    }
    fclose(out);
    return 0;
}
