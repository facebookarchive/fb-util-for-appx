//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <memory>
#include <string>

namespace facebook {
namespace appx {
    // An exception representing a POSIX error (ENOENT, etc.).
    class ErrnoException : public std::runtime_error
    {
    public:
        // Uses 'errno'.
        ErrnoException();
        explicit ErrnoException(const std::string &message);
        // Uses 'errno'.
        explicit ErrnoException(int error);
        ErrnoException(const std::string &message, int error);

        int error;
    };

    struct FileDeleter
    {
        void operator()(FILE *file)
        {
            if (file) {
                int rc = std::fclose(file);
                if (rc != 0) {
                    throw ErrnoException();
                }
            }
        }
    };

    typedef std::unique_ptr<FILE, FileDeleter> FilePtr;

    // Opens a file, like fopen.
    inline FilePtr Open(const std::string &path, const char *mode)
    {
        FilePtr file(std::fopen(path.c_str(), mode));
        if (!file) {
            throw ErrnoException(path);
        }
        return file;
    }

    // Seeks to a position in a file, like fseek.
    inline void Seek(const FilePtr &file, off_t pos, int whence)
    {
        if (fseeko(file.get(), pos, whence) != 0) {
            throw ErrnoException();
        }
    }

    // Reads bytes from a file, like fread.
    inline size_t Read(const FilePtr &file, std::size_t size, void *bytes)
    {
        size_t read = std::fread(bytes, 1, size, file.get());
        if (read < size && !std::feof(file.get())) {
            if (std::ferror(file.get())) {
                throw ErrnoException();
            } else {
                throw std::runtime_error("Incomplete fread, but not at EOF");
            }
        }
        return read;
    }

    // Writes bytes to a file, like fwrite.
    inline void Write(const FilePtr &file, std::size_t size, const void *bytes)
    {
        size_t written =
            std::fwrite(static_cast<const char *>(bytes), 1, size, file.get());
        if (written != size) {
            if (std::ferror(file.get())) {
                throw ErrnoException();
            } else {
                throw std::runtime_error("Incomplete fwrite");
            }
        }
    }

    // Copies all bytes (starting from the current position) from a file into a
    // sink.
    template <typename TSink>
    void Copy(const FilePtr &from, TSink &to)
    {
        std::uint8_t buffer[4096];
        for (;;) {
            std::size_t read = Read(from, sizeof(buffer), buffer);
            if (read == 0) {
                break;
            }
            to.Write(read, buffer);
        }
    }
}
}
