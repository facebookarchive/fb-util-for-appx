//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//

#include <APPX/File.h>
#include <APPX/Sign.h>
#include <APPX/Sink.h>
#include <APPX/ZIP.h>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace facebook {
namespace appx {
    namespace {
        // TODO(strager): Stream data instead of returning a chunk of memory.
        std::vector<std::uint8_t> GetSignatureBytes(PKCS7 *signature)
        {
            BIOPtr out(BIO_new(BIO_s_mem()));
            if (!out) {
                throw OpenSSLException();
            }
            if (!i2d_PKCS7_bio(out.get(), signature)) {
                throw OpenSSLException();
            }
            if (BIO_flush(out.get()) != 1) {
                throw OpenSSLException();
            }
            BUF_MEM *buffer;
            if (BIO_get_mem_ptr(out.get(), &buffer) < 0) {
                throw OpenSSLException();
            }
            const std::uint8_t *data =
                reinterpret_cast<const std::uint8_t *>(buffer->data);
            return std::vector<std::uint8_t>(data, data + buffer->length);
        }

        // Creates the AppxSignature.p7x file and inserts it into the ZIP.
        template <typename TSink>
        ZIPFileEntry WriteSignature(TSink &sink, const std::string &certPath,
                                    const APPXDigests &digests, off_t offset)
        {
            // AppxSignature.p7x *must* be DEFLATEd.
            std::vector<std::uint8_t> compressedSignatureData;
            std::uint32_t crc32;
            off_t uncompressedSize;
            {
                OpenSSLPtr<PKCS7, PKCS7_free> signature =
                    Sign(certPath, digests);
                std::vector<std::uint8_t> signatureData =
                    GetSignatureBytes(signature.get());

                VectorSink vectorSink(compressedSignatureData);
                auto deflateSink =
                    MakeDeflateSink(Z_BEST_COMPRESSION, vectorSink);
                CRC32Sink crc32Sink;
                OffsetSink offsetSink;
                auto sink = MakeMultiSink(deflateSink, crc32Sink, offsetSink);
                static const std::uint8_t p7xSignature[] = {0x50, 0x4b, 0x43,
                                                            0x58};
                sink.Write(sizeof(p7xSignature), p7xSignature);
                sink.Write(signatureData.size(), signatureData.data());
                deflateSink.Close();
                crc32 = crc32Sink.CRC32();
                uncompressedSize = offsetSink.Offset();
            }

            ZIPFileEntry entry(
                "AppxSignature.p7x",
                static_cast<off_t>(compressedSignatureData.size()),
                uncompressedSize, ZIPCompressionType::Deflate, offset, crc32,
                {}, SHA256Hash());
            entry.WriteFileRecordHeader(sink);
            sink.Write(compressedSignatureData.size(),
                       compressedSignatureData.data());
            return entry;
        }
    }

    void WriteAppx(
        const FilePtr &zip,
        const std::unordered_map<std::string, std::string> &fileNames,
        const std::string *certPath, int compressionLevel)
    {
        FileSink zipRawSink(zip.get());
        OffsetSink zipOffsetSink;
        auto zipSink = MakeMultiSink(zipRawSink, zipOffsetSink);
        std::vector<ZIPFileEntry> zipFileEntries;

        APPXDigests digests;

        // Write and hash the ZIP content.
        {
            SHA256Sink axpcSink;
            auto sink = MakeMultiSink(zipSink, axpcSink);
            for (const auto &fileNamePair : fileNames) {
                const std::string &archiveName = fileNamePair.first;
                const std::string &fileName = fileNamePair.second;
                zipFileEntries.emplace_back(
                    WriteZIPFileEntry(sink, zipOffsetSink.Offset(), fileName,
                                      archiveName, compressionLevel));
            }

            ZIPFileEntry blockMap = WriteAppxBlockMapZIPFileEntry(
                sink, zipOffsetSink.Offset(), zipFileEntries);
            digests.axbm = blockMap.sha256;
            zipFileEntries.emplace_back(std::move(blockMap));

            ZIPFileEntry contentTypes =
                WriteContentTypesZIPFileEntry(sink, zipOffsetSink.Offset());
            digests.axct = contentTypes.sha256;
            zipFileEntries.emplace_back(std::move(contentTypes));

            digests.axpc = axpcSink.SHA256();
        }

        // Hash (but do not write) the directory, pre-signature.
        {
            SHA256Sink axcdSink;
            OffsetSink tmpOffsetSink = zipOffsetSink;
            auto sink = MakeMultiSink(axcdSink, tmpOffsetSink);
            for (const ZIPFileEntry &entry : zipFileEntries) {
                entry.WriteDirectoryEntry(sink);
            }
            WriteZIPEndOfCentralDirectoryRecord(sink, tmpOffsetSink.Offset(),
                                                zipFileEntries);
            digests.axcd = axcdSink.SHA256();
        }

        // Sign and write the signature.
        if (certPath) {
            zipFileEntries.emplace_back(WriteSignature(
                zipSink, *certPath, digests, zipOffsetSink.Offset()));
        }

        // Write the directory.
        for (const ZIPFileEntry &entry : zipFileEntries) {
            entry.WriteDirectoryEntry(zipSink);
        }
        WriteZIPEndOfCentralDirectoryRecord(zipSink, zipOffsetSink.Offset(),
                                            zipFileEntries);
    }
}
}
