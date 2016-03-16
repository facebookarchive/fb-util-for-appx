//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//

#pragma once

#include <APPX/File.h>
#include <APPX/Hash.h>
#include <APPX/OpenSSL.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <type_traits>
#include <vector>
#include <zlib.h>

// A sink is an object to which bytes can be written.

namespace facebook {
namespace appx {
    // A sink which writes to a file.
    class FileSink
    {
    public:
        explicit FileSink(FILE *file) : file(file)
        {
        }

        void Write(std::size_t size, const std::uint8_t *bytes)
        {
            std::size_t written = std::fwrite(bytes, 1, size, this->file);
            if (written != size) {
                throw ErrnoException();
            }
        }

    private:
        FILE *file;
    };

    // A sink which creates a SHA256 digest.
    class SHA256Sink
    {
    public:
        SHA256Sink()
        {
            SHA256_Init(&this->context);
        }

        void Write(std::size_t size, const std::uint8_t *bytes)
        {
            SHA256_Update(&this->context, bytes, size);
        }

        SHA256Hash SHA256() const
        {
            SHA256_CTX context = this->context;
            std::uint8_t hash[SHA256_DIGEST_LENGTH];
            SHA256_Final(hash, &context);
            return SHA256Hash(hash);
        }

    private:
        SHA256_CTX context;
    };

    // A sink which encodes in base64.
    class Base64Sink
    {
    public:
        Base64Sink()
        {
            BIOPtr b64(BIO_new(BIO_f_base64()));
            if (!b64) {
                throw OpenSSLException();
            }

            BIOPtr mem(BIO_new(BIO_s_mem()));
            if (!mem) {
                throw OpenSSLException();
            }

            this->chain = BIOPtr(BIO_push(b64.get(), mem.get()));
            if (!this->chain) {
                throw OpenSSLException();
            }

            b64.release();
            mem.release();

            BIO_set_flags(this->chain.get(), BIO_FLAGS_BASE64_NO_NL);
        }

        void Write(std::size_t size, const std::uint8_t *bytes)
        {
            BIO_write(this->chain.get(), bytes, size);
        }

        void Close()
        {
            if (BIO_flush(this->chain.get()) != 1) {
                throw OpenSSLException();
            }
        }

        std::string Base64() const
        {
            BUF_MEM *bptr;
            BIO_get_mem_ptr(this->chain.get(), &bptr);
            if (bptr == nullptr) {
                throw OpenSSLException();
            }

            return std::string(reinterpret_cast<char *>(bptr->data),
                               bptr->length);
        }

    private:
        BIOPtr chain;
    };

    // A sink which feeds data to other sinks in equal-sized chunks.
    template <typename TSinkFactory>
    class ChunkSink
    {
    public:
        using Sink = typename std::result_of<TSinkFactory()>::type;

        ChunkSink(off_t chunkSize, TSinkFactory factory)
            : chunkSize(chunkSize), factory(factory), sink(this->factory())
        {
        }

        void Write(std::size_t size, const std::uint8_t *bytes)
        {
            while (size > 0) {
                std::size_t toWrite = static_cast<std::size_t>(std::min(
                    this->chunkSize - this->written, static_cast<off_t>(size)));
                this->sink.Write(toWrite, bytes);
                this->written += toWrite;
                bytes += toWrite;
                size -= toWrite;
                if (this->written == this->chunkSize) {
                    EndChunk();
                }
            }
        }

        void Close()
        {
            EndChunk();
            MaybeClose(this->sink);
        }

        const std::vector<Sink> &Chunks() const
        {
            return this->chunks;
        }

    private:
        void EndChunk()
        {
            if (this->written == 0) {
                return;
            }
            MaybeClose(this->sink);
            this->chunks.emplace_back(std::move(this->sink));
            this->sink = this->factory();
            this->written = 0;
        }

        template <typename TSink>
        static void MaybeClose(TSink &sink)
        {
            MaybeCloseImpl(sink, 0);
        }

        template <typename TSink>
        static auto MaybeCloseImpl(TSink &sink, int)
            -> decltype(sink.Close(), void())
        {
            sink.Close();
        }

        template <typename TSink>
        static void MaybeCloseImpl(TSink &sink, long)
        {
            // Do nothing.
        }

        off_t chunkSize;
        off_t written = 0;
        TSinkFactory factory;
        Sink sink;
        std::vector<Sink> chunks;
    };

    template <typename TSinkFactory>
    ChunkSink<TSinkFactory> MakeChunkSink(off_t chunkSize, TSinkFactory factory)
    {
        return ChunkSink<TSinkFactory>(chunkSize, factory);
    }

    // A sink which counts the number of bytes written, discarding the data.
    class OffsetSink
    {
    public:
        OffsetSink() : offset(0)
        {
        }

        explicit OffsetSink(off_t startOffset) : offset(startOffset)
        {
        }

        void Write(std::size_t size, const std::uint8_t *bytes)
        {
            this->offset += size;
        }

        off_t Offset() const
        {
            return this->offset;
        }

    private:
        off_t offset;
    };

    // A sink which appends to a byte vector.
    class VectorSink
    {
    public:
        VectorSink(std::vector<uint8_t> &vector) : vector(vector)
        {
        }

        void Write(std::size_t size, const std::uint8_t *bytes)
        {
            this->vector.insert(this->vector.end(), bytes, bytes + size);
        }

    private:
        std::vector<std::uint8_t> &vector;
    };

    // A sink which compresses into another sink using the ZIP DEFLATE
    // algorithm. Close must be called after writing data.
    template <typename TSink>
    class DeflateSink
    {
    public:
        DeflateSink(int compressionLevel, TSink &sink) : sink(&sink)
        {
            this->stream.zalloc = nullptr;
            this->stream.zfree = nullptr;
            this->stream.opaque = nullptr;
            int rc =
                deflateInit2(&this->stream, compressionLevel, Z_DEFLATED,
                             -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
            if (rc != Z_OK) {
                throw std::runtime_error("deflateInit failed");
            }
        }

        ~DeflateSink()
        {
            if (this->sink) {
                int rc = deflateEnd(&this->stream);
                if (rc != Z_OK) {
                    __builtin_trap();
                    throw std::runtime_error("deflateEnd failed");
                }
            } else {
                // This object is moved-from.
            }
        }

        // z_stream is not copyable.
        DeflateSink(const DeflateSink &) = delete;

        DeflateSink &operator=(const DeflateSink &) = delete;

        // z_stream is movable.
        DeflateSink(DeflateSink &&other) : sink(other.sink)
        {
            other.sink = nullptr;  // See ~DeflateSink.
            std::swap(this->stream, other.stream);
        }

        DeflateSink &operator=(DeflateSink &&other)
        {
            this->sink = other.sink;
            other.sink = nullptr;  // See ~DeflateSink.
            std::swap(this->stream, other.stream);
            return *this;
        }

        void Write(std::size_t size, const std::uint8_t *bytes)
        {
            if (size > 0) {
                isEmpty = false;
            }

            this->stream.next_in = const_cast<std::uint8_t *>(bytes);
            this->stream.avail_in = size;
            this->Deflate(Z_NO_FLUSH);
        }

        void Close()
        {
            this->stream.next_in = nullptr;
            this->stream.avail_in = 0;
            this->Deflate(Z_FINISH);
        }

        void Flush()
        {
            if (!isEmpty) {
                this->stream.next_in = nullptr;
                this->stream.avail_in = 0;
                this->Deflate(Z_FULL_FLUSH);
            }
        }

    private:
        void Deflate(int flushMode)
        {
            std::uint8_t buffer[1024];
            do {
                this->stream.next_out = buffer;
                this->stream.avail_out = sizeof(buffer);
                int rc = deflate(&this->stream, flushMode);
                if (rc == Z_STREAM_ERROR) {
                    throw std::runtime_error("deflate failed");
                }
                this->sink->Write(sizeof(buffer) - this->stream.avail_out,
                                  buffer);
            } while (this->stream.avail_out == 0);
        }

        TSink *sink;
        z_stream stream;
        bool isEmpty = true;
    };

    template <typename TSink>
    DeflateSink<TSink> MakeDeflateSink(int compressionLevel, TSink &sink)
    {
        return DeflateSink<TSink>(compressionLevel, sink);
    }

    // A sink which creates a CRC32 digest.
    class CRC32Sink
    {
    public:
        void Write(std::size_t size, const std::uint8_t *bytes)
        {
            if (sizeof(size) > std::numeric_limits<unsigned int>::max()) {
                // TODO(strager): Support larger inputs.
                throw std::range_error(
                    "Buffer is too big for zlib's crc32 function");
            }
            this->crc =
                crc32(this->crc, bytes, static_cast<unsigned int>(size));
        }

        std::uint32_t CRC32() const
        {
            return this->crc;
        }

    private:
        std::uint32_t crc = crc32(0, nullptr, 0);
    };

    // A linked list of sinks.
    //
    // This definition is the end-of-list base case.
    template <typename...>
    class MultiSink
    {
    public:
        MultiSink()
        {
        }

        void Write(std::size_t size, const std::uint8_t *bytes)
        {
            // Do nothing.
        }
    };

    // A linked list of sinks.
    //
    // This definition is a specialization for non-empty lists.
    template <typename THeadSink, typename... TTailSinks>
    class MultiSink<THeadSink, TTailSinks...>
    {
    public:
        MultiSink(THeadSink &head, TTailSinks &... tail)
            : head(head), tail(tail...)
        {
        }

        void Write(std::size_t size, const std::uint8_t *bytes)
        {
            head.Write(size, bytes);
            tail.Write(size, bytes);
        }

    private:
        THeadSink &head;
        MultiSink<TTailSinks...> tail;
    };

    // Constructs a MultiSink object from the given sinks.
    template <typename... TSinks>
    MultiSink<TSinks...> MakeMultiSink(TSinks &... sinks)
    {
        return MultiSink<TSinks...>(sinks...);
    }
}
}
