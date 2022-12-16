#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include "windows.h"
#include "psapi.h"

intptr_t find_pattern(
        std::vector<unsigned char> &data,
        intptr_t base,
        const unsigned char *pattern,
        const char *mask,
        intptr_t offset,
        intptr_t usage);

intptr_t find_pattern(
        HMODULE module,
        const unsigned char *pattern,
        const char *mask,
        intptr_t offset,
        intptr_t usage);

intptr_t replace_pattern(
        HMODULE module,
        const unsigned char *pattern,
        const char *mask,
        intptr_t offset,
        intptr_t usage,
        const unsigned char *replace_data,
        const char *replace_mask);

intptr_t replace_pattern(
        HMODULE module,
        const std::string &signature,
        const std::string &replacement,
        intptr_t offset,
        intptr_t usage);
