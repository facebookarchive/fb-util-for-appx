//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//

#pragma once

#include <APPX/Encode.h>
#include <APPX/File.h>
#include <APPX/Hash.h>
#include <APPX/Sink.h>
#include <APPX/XML.h>
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <zlib.h>

namespace facebook {
namespace appx {
    // Timestamp for files in ZIP archives. Hard-coded so archiving is
    // deterministic.
    enum
    {
        kFileTime = 0x8706,
        kFileDate = 0x4722,
    };

    // Magic archiver numbers for files in the ZIP archives.
    enum
    {
        kArchiverVersion = 45,
        kFileExtractVersion = 20,
        kArchiveExtractVersion = 45,
    };

    enum class ZIPCompressionType : std::uint16_t
    {
        Store = 0,
        Deflate = 8,
    };

    // Metadata for a block in AppxBlockMap.xml.
    struct ZIPBlock
    {
        // Size of a block, in uncompressed bytes.
        // https://msdn.microsoft.com/en-us/library/windows/desktop/jj709947.aspx
        enum
        {
            kSize = 65536
        };

        ZIPBlock(SHA256Hash sha256, off_t compressedSize = kNotCompressed)
            : sha256(sha256), compressedSize(compressedSize)
        {
        }

        // Hash of the uncompressed data.
        SHA256Hash sha256;

        // If kNotCompressed, the chunk is not compressed.
        off_t compressedSize;
        enum : off_t
        {
            kNotCompressed = -1
        };
    };

    struct ZIPFileEntry
    {
        std::string fileName;
        std::string sanitizedFileName;
        off_t compressedSize;
        off_t uncompressedSize;
        ZIPCompressionType compressionType;
        off_t fileRecordHeaderOffset;
        std::uint32_t crc32;

        // For normal files.
        std::vector<ZIPBlock> blocks;
        // For [Content_Types].xml and AppxBlockMap.xml.
        SHA256Hash sha256;

        ZIPFileEntry(std::string fileName, off_t compressedSize,
                     off_t uncompressedSize, ZIPCompressionType compressionType,
                     off_t fileRecordHeaderOffset, std::uint32_t crc32,
                     std::vector<ZIPBlock> blocks, SHA256Hash sha256)
            : fileName(fileName),
              sanitizedFileName(SanitizedFileName(fileName)),
              compressedSize(compressedSize),
              uncompressedSize(uncompressedSize),
              compressionType(compressionType),
              fileRecordHeaderOffset(fileRecordHeaderOffset),
              crc32(crc32),
              blocks(blocks),
              sha256(sha256)
        {
        }

        ZIPFileEntry(std::string fileName, off_t size,
                     off_t fileRecordHeaderOffset, std::uint32_t crc32,
                     std::vector<ZIPBlock> blocks, SHA256Hash sha256)
            : ZIPFileEntry(fileName, size, size, ZIPCompressionType::Store,
                           fileRecordHeaderOffset, crc32, blocks, sha256)
        {
        }

        static std::string SanitizedFileName(const std::string &fileName);

        off_t FileRecordHeaderSize() const
        {
            return 30 + this->sanitizedFileName.size();
        }

        off_t FileRecordSize() const
        {
            return this->FileRecordHeaderSize() + this->compressedSize;
        }

        template <typename TSink>
        void WriteFileRecordHeader(TSink &sink) const
        {
            std::uint8_t data[] = {
                FB_BYTES_4_LE(0x04034B50),  // Signature.
                FB_BYTES_2_LE(kFileExtractVersion),
                FB_BYTES_2_LE(0),  // Flags.
                FB_BYTES_2_LE(
                    static_cast<std::uint16_t>(this->compressionType)),
                FB_BYTES_2_LE(kFileTime), FB_BYTES_2_LE(kFileDate),
                FB_BYTES_4_LE(this->crc32), FB_BYTES_4_LE(this->compressedSize),
                FB_BYTES_4_LE(this->uncompressedSize),
                FB_BYTES_2_LE(this->sanitizedFileName.size()),
                FB_BYTES_2_LE(0),  // Extra field length.
            };
            sink.Write(sizeof(data), data);
            sink.Write(this->sanitizedFileName.size(),
                       reinterpret_cast<const std::uint8_t *>(
                           this->sanitizedFileName.c_str()));
        }

        off_t DirectoryEntrySize() const
        {
            return 46 + this->sanitizedFileName.size();
        }

        template <typename TSink>
        void WriteDirectoryEntry(TSink &sink) const
        {
            std::uint8_t data[] = {
                FB_BYTES_4_LE(0x02014B50),  // Signature.
                FB_BYTES_2_LE(kArchiverVersion),
                FB_BYTES_2_LE(kFileExtractVersion),
                FB_BYTES_2_LE(0),  // Flags.
                FB_BYTES_2_LE(
                    static_cast<std::uint16_t>(this->compressionType)),
                FB_BYTES_2_LE(kFileTime), FB_BYTES_2_LE(kFileDate),
                FB_BYTES_4_LE(this->crc32), FB_BYTES_4_LE(this->compressedSize),
                FB_BYTES_4_LE(this->uncompressedSize),
                FB_BYTES_2_LE(this->sanitizedFileName.size()),
                FB_BYTES_2_LE(0),  // Extra field length.
                FB_BYTES_2_LE(0),  // File comment length.
                FB_BYTES_2_LE(0),  // Disk number start.
                FB_BYTES_2_LE(0),  // Internal file attributes.
                FB_BYTES_4_LE(0),  // External file attributes.
                FB_BYTES_4_LE(this->fileRecordHeaderOffset),
            };
            sink.Write(sizeof(data), data);
            sink.Write(this->sanitizedFileName.size(),
                       reinterpret_cast<const std::uint8_t *>(
                           this->sanitizedFileName.c_str()));
        }
    };

    inline bool _IsAPPXFile(const std::string &inputFileName)
    {
        const std::string suffix = ".appx";
        return (
            suffix.size() < inputFileName.size() &&
            std::equal(suffix.rbegin(), suffix.rend(), inputFileName.rbegin()));
    }

    // For each of the appx files that we store in appxbundle, there is a
    // corresponding
    //   entry in AppxBundleManifest.xml. This entry (an XML node) contains the
    //   "Offset"
    //   property which specifies the header offset in the final appxbundle
    //   file. Since this
    //   value is not known before we create the appxbundle, we provide a
    //   placeholder
    //   that looks like "FileName.appx-offset". This placeholder suggests that
    //   when packaging
    //   and creating the appxbundle we need to replace "FileName.appx-offset"
    //   with the
    //   number that represents the offset for FileName.appx.
    inline std::string _ManifestContentsAfterPopulatingOffsets(
        const std::string &manifestInputFileName,
        const std::vector<ZIPFileEntry> &otherEntries)
    {
        std::ifstream manifestInput(manifestInputFileName);
        std::string manifestText(
            (std::istreambuf_iterator<char>(manifestInput)),
            std::istreambuf_iterator<char>());

        // here we are creating offsets
        for (const ZIPFileEntry &entry : otherEntries) {
            std::string offsetTemplateName = entry.fileName + "-offset";
            auto pos = 0;
            auto entryDataOffset =
                entry.fileRecordHeaderOffset + entry.FileRecordHeaderSize();
            while ((pos = manifestText.find(offsetTemplateName, pos)) !=
                   std::string::npos) {
                manifestText.replace(pos, offsetTemplateName.length(),
                                     std::to_string(entryDataOffset));
            }
        }

        return manifestText;
    }

    template <typename TSink>
    void WriteZIPEndOfCentralDirectoryRecord(
        TSink &sink, off_t offset, const std::vector<ZIPFileEntry> &entries)
    {
        std::uint64_t directoryEntriesSize = 0;
        std::uint64_t fileRecordsSize = 0;
        for (const ZIPFileEntry &entry : entries) {
            directoryEntriesSize += entry.DirectoryEntrySize();
            fileRecordsSize += entry.FileRecordSize();
        }
        off_t centralDirectoryEndOffset = offset;
        std::uint8_t data[] = {
            // ZIP64 central directory end.
            FB_BYTES_4_LE(0x06064B50),  // Signature.
            FB_BYTES_8_LE(56 - 12),     // Size of this record after this field.
            FB_BYTES_2_LE(kArchiverVersion),
            FB_BYTES_2_LE(kArchiveExtractVersion),
            FB_BYTES_4_LE(0),  // Index of this disk.
            FB_BYTES_4_LE(0),  // Index of disk with central directory start.
            FB_BYTES_8_LE(entries.size()),  // Entries in this disk.
            FB_BYTES_8_LE(entries.size()),  // Entries in central directory.
            FB_BYTES_8_LE(directoryEntriesSize),
            FB_BYTES_8_LE(fileRecordsSize),  // Offset of directory start.
            // ZIP64 central directory locator.
            FB_BYTES_4_LE(0x07064B50),  // Signature.
            FB_BYTES_4_LE(0),  // Index of disk with central directory end.
            FB_BYTES_8_LE(centralDirectoryEndOffset),
            FB_BYTES_4_LE(1),  // Number of disks.
            // Central directory record.
            FB_BYTES_4_LE(0x06054B50),  // Signature.
            FB_BYTES_2_LE(0),           // Index of this disk.
            FB_BYTES_2_LE(0),  // Index of disk with central directory start.
            FB_BYTES_4_LE(0xFFFFFFFF),  // Entries in this disk.
            FB_BYTES_4_LE(0xFFFFFFFF),  // Entries in central directory.
            FB_BYTES_4_LE(0xFFFFFFFF),  // Central directory start offset.
            FB_BYTES_2_LE(0),           // Comment length.
        };
        sink.Write(sizeof(data), data);
    }

    // this writes [Content_Types].xml
    template <typename TSink>
    ZIPFileEntry WriteContentTypesZIPFileEntry(
        TSink &sink, off_t offset, bool isBundle,
        const std::vector<ZIPFileEntry> &otherEntries)
    {
        // we only need the filenames from otherEntries
        // [Content_Types].xml contains the ZIP-escaped
        // names, hence the use of sanitizedFileName
        std::unordered_map<std::string, const char *> sKnownContentTypes = {
            {"appx", "application/vnd.ms-appx"},
            {"dll", "application/x-msdownload"},
                {"exe", "application/x-msdownload"},
                {"png", "image/png"},
            };
        if (isBundle) {
            sKnownContentTypes.emplace(
                "xml", "application/vnd.ms-appx.bundlemanifest+xml");
        } else {
            sKnownContentTypes.emplace("xml",
                                       "application/vnd.ms-appx.manifest+xml");
        }

        static const char *kDefaultContentType = "application/octet-stream";

        std::ostringstream ss;
        ss << "<?xml "
           << "version=\"1.0\" "
           << "encoding=\"UTF-8\" "
           << "standalone=\"yes\"?>\r\n";
        ss << "<Types "
           << "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
              "content-types\">";

        std::vector<std::string> writtenExtensions;
        for (const ZIPFileEntry &entry : otherEntries) {
            std::size_t baseNamePos = entry.sanitizedFileName.rfind('/') + 1;
            std::size_t extensionPos = entry.sanitizedFileName.rfind('.') + 1;
            bool hasExtension = extensionPos > baseNamePos;
            if (hasExtension) {
                std::string extension(
                    entry.sanitizedFileName.cbegin() + extensionPos,
                    entry.sanitizedFileName.cend());
                bool notWritten =
                    std::find(writtenExtensions.begin(),
                              writtenExtensions.end(),
                              extension) == writtenExtensions.end();
                if (notWritten) {
                    auto contentTypeIt = sKnownContentTypes.find(extension);
                    const char *contentType;
                    if (contentTypeIt != sKnownContentTypes.end()) {
                        contentType = contentTypeIt->second;
                    } else {
                        contentType = kDefaultContentType;
                    }
                    ss << "<Default "
                       << "Extension=\"" << XMLEncodeString(extension) << "\" "
                       << "ContentType=\"" << XMLEncodeString(contentType)
                       << "\"/>";
                    writtenExtensions.push_back(extension);
                }
            } else {
                ss << "<Override "
                   << "PartName=\"/" << XMLEncodeString(entry.sanitizedFileName)
                   << "\" "
                   << "ContentType=\"" << XMLEncodeString(kDefaultContentType)
                   << "\"/>";
            }
        }

        ss << "<Override "
           << "PartName=\"/AppxBlockMap.xml\" "
           << "ContentType=\"application/vnd.ms-appx.blockmap+xml\"/>";
        ss << "<Override "
           << "PartName=\"/AppxSignature.p7x\" "
           << "ContentType=\"application/vnd.ms-appx.signature\"/>";
        ss << "<Override "
           << "PartName=\"/AppxMetadata/CodeIntegrity.cat\" "
           << "ContentType=\"application/vnd.ms-pkiseccat\"/>";
        ss << "</Types>";

        const std::string xml(ss.str());
        const std::uint8_t *xmlBytes =
            reinterpret_cast<const std::uint8_t *>(xml.c_str());
        std::size_t xmlSize = xml.size();
        CRC32Sink crc32Sink;
        crc32Sink.Write(xmlSize, xmlBytes);
        assert(xmlSize < std::numeric_limits<off_t>::max());
        ZIPFileEntry entry("[Content_Types].xml", static_cast<off_t>(xmlSize),
                           offset, crc32Sink.CRC32(), {},
                           SHA256Hash::DigestFromBytes(xmlSize, xmlBytes));
        entry.WriteFileRecordHeader(sink);
        sink.Write(xmlSize, xmlBytes);
        return entry;
    }

    template <typename TSink>
    ZIPFileEntry WriteAppxBlockMapZIPFileEntry(
        TSink &sink, off_t offset,
        const std::vector<ZIPFileEntry> &otherEntries, bool isBundle)
    {
        // https://msdn.microsoft.com/en-us/library/windows/desktop/jj709951.aspx
        std::ostringstream ss;
        ss << "<?xml "
           << "version=\"1.0\" "
           << "encoding=\"UTF-8\" "
           << "standalone=\"no\"?>\r\n";
        ss << "<BlockMap "
           << "xmlns=\"http://schemas.microsoft.com/appx/2010/blockmap\" "
           << "HashMethod=\"http://www.w3.org/2001/04/xmlenc#sha256\">";
        for (const ZIPFileEntry &entry : otherEntries) {
            if (isBundle && _IsAPPXFile(entry.fileName)) {
                continue;
            }
            std::string fixedFileName = entry.fileName;
            std::replace(fixedFileName.begin(), fixedFileName.end(), '/', '\\');
            ss << "<File "
               << "Name=\"" << XMLEncodeString(fixedFileName) << "\" "
               << "Size=\"" << entry.uncompressedSize << "\" "
               << "LfhSize=\"" << entry.FileRecordHeaderSize() << "\">";

            for (const ZIPBlock &block : entry.blocks) {
                Base64Sink base64Sink;
                base64Sink.Write(sizeof(block.sha256.bytes),
                                 block.sha256.bytes);
                base64Sink.Close();

                ss << "<Block Hash=\"" << base64Sink.Base64() << "\"";
                if (block.compressedSize != ZIPBlock::kNotCompressed) {
                    // FIXME(strager): Ensure locales don't screw us over.
                    ss << " Size=\"" << block.compressedSize << "\"";
                }
                ss << "/>";
            }
            ss << "</File>";
        }
        ss << "</BlockMap>";
        std::string xml = ss.str();
        std::size_t xmlSize = xml.size();
        const std::uint8_t *xmlBytes =
            reinterpret_cast<const std::uint8_t *>(xml.c_str());
        std::uint32_t crc32;
        SHA256Hash sha256;
        {
            CRC32Sink crc32Sink;
            SHA256Sink sha256Sink;
            auto sink = MakeMultiSink(crc32Sink, sha256Sink);
            sink.Write(xmlSize, xmlBytes);
            crc32 = crc32Sink.CRC32();
            sha256 = sha256Sink.SHA256();
        }
        assert(xml.size() < std::numeric_limits<off_t>::max());
        ZIPFileEntry entry("AppxBlockMap.xml", static_cast<off_t>(xmlSize),
                           offset, crc32, {}, sha256);
        entry.WriteFileRecordHeader(sink);
        sink.Write(xmlSize, xmlBytes);
        return entry;
    }

    template <typename TSink>
    ZIPFileEntry WriteAppxBundleManifestZIPFileEntry(
        TSink &sink, off_t offset, const std::string &inputFileName,
        const std::string &archiveFileName, int compressionLevel,
        const std::vector<ZIPFileEntry> &otherEntries)
    {
        return WriteZIPFileEntry(
            sink, offset, archiveFileName, compressionLevel,
            [&inputFileName, &otherEntries](auto &sink) {
                std::string manifestText =
                    _ManifestContentsAfterPopulatingOffsets(inputFileName,
                                                            otherEntries);
                sink.Write(manifestText.size(),
                           reinterpret_cast<const std::uint8_t *>(
                               manifestText.c_str()));
            });
    }

    // Write the ZIP file record header and data to sink, reading the data using
    // dataCallback.
    //
    // dataCallback is called as a function:
    // template <typename TSink> void dataCallback(TSink &);
    //
    // dataCallback is called at most once.
    template <typename TSink, typename TSource>
    ZIPFileEntry WriteZIPFileEntry(TSink &sink, off_t offset,
                                   const std::string &archiveFileName,
                                   int compressionLevel, TSource &&dataCallback)
    {
        std::uint32_t crc32;
        off_t uncompressedFileSize;
        off_t compressedFileSize;
        std::vector<uint8_t> data;
        std::vector<ZIPBlock> blocks;
        ZIPCompressionType compressionType;
        {
            if (_IsAPPXFile(archiveFileName)) {
                compressionLevel = Z_NO_COMPRESSION;
            }

            CRC32Sink crc32Sink;
            VectorSink dataSink(data);
            // TODO(strager): Instead of writing the data to memory, write the
            // header after the data.
            if (compressionLevel == Z_NO_COMPRESSION) {
                OffsetSink offsetSink;
                auto chunkSink = MakeChunkSink(ZIPBlock::kSize,
                                               []() { return SHA256Sink(); });
                auto sink =
                    MakeMultiSink(crc32Sink, offsetSink, dataSink, chunkSink);
                dataCallback(sink);
                chunkSink.Close();
                for (const SHA256Sink &chunk : chunkSink.Chunks()) {
                    blocks.push_back(ZIPBlock(chunk.SHA256()));
                }
                uncompressedFileSize = offsetSink.Offset();
                compressedFileSize = uncompressedFileSize;
                compressionType = ZIPCompressionType::Store;
            } else {
                OffsetSink compressedOffsetSink;
                auto targetSink = MakeMultiSink(dataSink, compressedOffsetSink);
                struct Chunk
                {
                    Chunk(DeflateSink<decltype(targetSink)> &deflateSink,
                          OffsetSink &deflateOffsetSink)
                        : deflateSink(&deflateSink),
                          deflateOffsetSink(&deflateOffsetSink)
                    {
                        this->startOffset = this->deflateOffsetSink->Offset();
                    }

                    void Write(std::size_t size, const std::uint8_t *bytes)
                    {
                        this->sha256Sink.Write(size, bytes);
                        this->deflateSink->Write(size, bytes);
                    }

                    void Close()
                    {
                        this->deflateSink->Flush();
                        this->endOffset = this->deflateOffsetSink->Offset();
                    }

                    off_t CompressedSize() const
                    {
                        return this->endOffset - this->startOffset;
                    }

                    SHA256Hash SHA256() const
                    {
                        return this->sha256Sink.SHA256();
                    }

                private:
                    SHA256Sink sha256Sink;
                    DeflateSink<decltype(targetSink)> *deflateSink;
                    OffsetSink *deflateOffsetSink;
                    off_t startOffset;
                    off_t endOffset;
                };
                auto deflateSink =
                    MakeDeflateSink(Z_BEST_COMPRESSION, targetSink);
                auto chunkSink = MakeChunkSink(
                    ZIPBlock::kSize, [&deflateSink, &compressedOffsetSink]() {
                        return Chunk(deflateSink, compressedOffsetSink);
                    });
                OffsetSink uncompressedOffsetSink;
                auto sink =
                    MakeMultiSink(chunkSink, uncompressedOffsetSink, crc32Sink);
                dataCallback(sink);
                chunkSink.Close();
                deflateSink.Close();
                for (const Chunk &chunk : chunkSink.Chunks()) {
                    blocks.push_back(
                        ZIPBlock(chunk.SHA256(), chunk.CompressedSize()));
                }
                uncompressedFileSize = uncompressedOffsetSink.Offset();
                compressedFileSize = compressedOffsetSink.Offset();
                compressionType = ZIPCompressionType::Deflate;
            }
            crc32 = crc32Sink.CRC32();
        }
        ZIPFileEntry entry(archiveFileName, compressedFileSize,
                           uncompressedFileSize, compressionType, offset, crc32,
                           blocks, SHA256Hash());
        entry.WriteFileRecordHeader(sink);
        sink.Write(data.size(), data.data());
        return entry;
    }

    // Write the ZIP file record header and data to sink, reading the data from
    // a file.
    template <typename TSink>
    ZIPFileEntry WriteZIPFileEntry(TSink &sink, off_t offset,
                                   const std::string &inputFileName,
                                   const std::string &archiveFileName,
                                   int compressionLevel)
    {
        return WriteZIPFileEntry(sink, offset, archiveFileName,
                                 compressionLevel,
                                 [&inputFileName](auto &sink) {
                                     FilePtr file = Open(inputFileName, "rb");
                                     Copy(file, sink);
                                 });
    }
}
}
