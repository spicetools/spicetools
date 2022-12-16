#include "utils.h"
#include "avs/core.h"
#include "util/utils.h"


namespace layeredfs {

    char *snprintf_auto(const char *fmt, ...) {
        va_list argList;

        va_start(argList, fmt);
        size_t len = vsnprintf(NULL, 0, fmt, argList);
        auto s = (char *) malloc(len + 1);
        vsnprintf(s, len + 1, fmt, argList);
        va_end(argList);
        return s;
    }

    int string_ends_with(const char *str, const char *suffix) {
        size_t str_len = strlen(str);
        size_t suffix_len = strlen(suffix);

        return
                (str_len >= suffix_len) &&
                (0 == strcmp(str + (str_len - suffix_len), suffix));
    }

    void string_replace(std::string &str, const char *from, const char *to) {
        auto to_len = strlen(to);
        auto from_len = strlen(from);
        size_t offset = 0;
        for (auto pos = str.find(from); pos != std::string::npos; pos = str.find(from, offset)) {
            str.replace(pos, from_len, to);
            // avoid recursion if to contains from
            offset = pos + to_len;
        }
    }

    wchar_t *str_widen(const char *src) {
        int nchars;
        wchar_t *result;

        nchars = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);

        if (!nchars) {
            abort();
        }

        result = (wchar_t *) malloc(nchars * sizeof(wchar_t));

        if (!MultiByteToWideChar(CP_ACP, 0, src, -1, result, nchars)) {
            abort();
        }

        return result;
    }

    bool file_exists(const char *name) {
        auto res = avs::core::avs_fs_open(name, 1, 420);
        if (res > 0)
            avs::core::avs_fs_close(res);
        return res > 0;
    }

    bool folder_exists(const char *name) {
        WIN32_FIND_DATAA ffd;
        HANDLE hFind = FindFirstFileA(name, &ffd);

        if (hFind == INVALID_HANDLE_VALUE || !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            return false;
        }
        FindClose(hFind);
        return true;
    }

    time_t file_time(const char *path) {
        auto wide = str_widen(path);
        auto hFile = CreateFileW(wide,  // file to open
                                 GENERIC_READ,          // open for reading
                                 FILE_SHARE_READ,       // share for reading
                                 NULL,                  // default security
                                 OPEN_EXISTING,         // existing file only
                                 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // normal file
                                 NULL);                 // no attr. template
        free(wide);
        if (hFile == INVALID_HANDLE_VALUE)
            return 0;

        FILETIME mtime;
        GetFileTime(hFile, NULL, NULL, &mtime);
        CloseHandle(hFile);

        ULARGE_INTEGER result;
        result.LowPart = mtime.dwLowDateTime;
        result.HighPart = mtime.dwHighDateTime;
        return result.QuadPart;
    }

    LONG time(void) {
        SYSTEMTIME time;
        GetSystemTime(&time);
        return (time.wSecond * 1000) + time.wMilliseconds;
    }

    uint8_t *lz_compress(uint8_t *input, size_t input_length, size_t *compressed_length) {

        // check if cstream is unavailable
        if (avs::core::cstream_create == nullptr) {
            return lz_compress_dummy(input, input_length, compressed_length);
        } else {

            /*
             * Compression using cstream
             */

            auto compressor = avs::core::cstream_create(avs::core::CSTREAM_AVSLZ_COMPRESS);
            if (!compressor) {
                logf("Couldn't create");
                return NULL;
            }
            compressor->in_buf = input;
            compressor->in_size = (uint32_t) input_length;
            // worst case, for every 8 bytes there will be an extra flag byte
            auto to_add = MAX(input_length / 8, 1);
            auto compress_size = input_length + to_add;
            auto compress_buffer = (unsigned char*)malloc(compress_size);
            compressor->out_buf = compress_buffer;
            compressor->out_size = (uint32_t) compress_size;

            bool ret;
            ret = avs::core::cstream_operate(compressor);
            if (!ret && !compressor->in_size) {
                compressor->in_buf = NULL;
                compressor->in_size = -1;
                ret = avs::core::cstream_operate(compressor);
            }
            if (!ret) {
                logf("Couldn't operate");
                return NULL;
            }
            if (avs::core::cstream_finish(compressor)) {
                logf("Couldn't finish");
                return NULL;
            }
            *compressed_length = compress_size - compressor->out_size;
            avs::core::cstream_destroy(compressor);
            return compress_buffer;
        }
    }

    uint8_t *lz_compress_dummy(uint8_t *input, size_t input_length, size_t *compressed_length) {
        uint8_t *output = (uint8_t *) malloc(input_length + input_length / 8 + 9);
        uint8_t *cur_byte = &output[0];

        // copy data blocks
        for (size_t n = 0; n < input_length / 8; n++) {

            // fake flag
            *cur_byte++ = 0xFF;

            // uncompressed data
            for (size_t i = 0; i < 8; i++) {
                *cur_byte++ = input[n * 8 + i];
            }
        }

        // remaining bytes
        int extra_bytes = input_length % 8;
        if (extra_bytes == 0) {
            *cur_byte++ = 0x00;
        } else {
            *cur_byte++ = 0xFF >> (8 - extra_bytes);
            for (size_t i = input_length - extra_bytes; i < input_length; i++) {
                *cur_byte++ = input[i];
            }
            for (size_t i = 0; i < 4; i++) {
                *cur_byte++ = 0x00;
            }
        }

        // calculate size
        *compressed_length = (size_t) (cur_byte - &output[0]);
        return output;
    }
}
