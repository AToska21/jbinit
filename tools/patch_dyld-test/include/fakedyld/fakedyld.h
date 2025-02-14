#ifndef MOCK_FAKEDYLD_H
#define MOCK_FAKEDYLD_H

#if __STDC_HOSTED__ != 1
#error "this file should not be used in the real ramdisk"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>

typedef struct {
    char* file_p;
    size_t file_len;
} memory_file_handle_t;

void patch_dyld(memory_file_handle_t* dyld_handle, int platform);
static inline void spin(void) { fprintf(stderr, "spin() called!\n"); exit(1); }

#define LOG(...) printf(__VA_ARGS__)

#endif
