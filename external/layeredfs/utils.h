#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include <string>

#include "util/logging.h"
#include "config.h"

#define logf(fmt,...) {char*b=snprintf_auto(fmt,##__VA_ARGS__);log_misc("layeredfs","{}",b?b:":(");if(b)free(b);} void()
#define logf_verbose(...) if (config.verbose_logs) {logf(__VA_ARGS__);} void()

namespace layeredfs {

    char *snprintf_auto(const char *fmt, ...);
    int string_ends_with(const char *str, const char *suffix);
    void string_replace(std::string &str, const char *from, const char *to);
    wchar_t *str_widen(const char *src);
    bool file_exists(const char *name);
    bool folder_exists(const char *name);
    time_t file_time(const char *path);
    LONG time(void);
    uint8_t *lz_compress(uint8_t *input, size_t input_length, size_t *compressed_length);
    uint8_t *lz_compress_dummy(uint8_t *input, size_t input_length, size_t *compressed_length);
}
