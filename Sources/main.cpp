//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//

#include <APPX/APPX.h>
#include <APPX/File.h>
#include <cassert>
#include <exception>
#include <fts.h>
#include <memory>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using namespace facebook::appx;

namespace {
struct FTSDeleter
{
    void operator()(FTS *fs)
    {
        if (fs) {
            int rc = fts_close(fs);
            if (rc != 0) {
                throw ErrnoException();
            }
        }
    }
};

// Get the archive name (i.e. the path excluding the top level directory) of
// an fts traversal entry.
std::string GetArchiveName(FTSENT *ent)
{
    std::vector<const char *> components;
    components.reserve(ent->fts_level);
    size_t namesLength = 0;
    // Use do-while to allow files with fts_level == 0.
    do {
        components.push_back(ent->fts_name);
        namesLength += ent->fts_namelen;
        ent = ent->fts_parent;
    } while (ent && ent->fts_level > 0);
    size_t expectedLength =
        (components.size() == 0 ? 0 : components.size() - 1) + namesLength;
    std::string archiveName;
    archiveName.reserve(expectedLength);
    bool insertSlash = false;
    for (auto i = components.rbegin(); i != components.rend(); ++i) {
        if (insertSlash) {
            archiveName += "/";
        }
        archiveName += *i;
        insertSlash = true;
    }
    assert(archiveName.size() == expectedLength);
    return archiveName;
}

// Given the path to a file or directory, add files to a mapping from archive
// names to local filesystem paths.
void GetArchiveFileList(const char *path,
                        std::unordered_map<std::string, std::string> &fileNames)
{
    char *const paths[] = {const_cast<char *>(path), nullptr};
    std::unique_ptr<FTS, FTSDeleter> fs(
        fts_open(paths, FTS_NOSTAT | FTS_PHYSICAL, nullptr));
    if (!fs) {
        throw ErrnoException();
    }
    while (FTSENT *ent = fts_read(fs.get())) {
        switch (ent->fts_info) {
            case FTS_ERR:
            case FTS_DNR:
                throw ErrnoException(ent->fts_path, ent->fts_errno);

            case FTS_D:
            case FTS_DOT:
            case FTS_DP:
                // Ignore directories.
                break;

            case FTS_DEFAULT:
            case FTS_F:
            case FTS_NS:
            case FTS_NSOK:
            case FTS_SL:
            case FTS_SLNONE:
                fileNames.insert(std::make_pair(GetArchiveName(ent),
                                                std::string(ent->fts_path)));
                break;

            default:
                throw std::runtime_error("Unknown FTS info");
        }
    }
}

void PrintUsage(const char *programName)
{
    fprintf(stderr,
            "Usage: %s -o APPX [OPTION]... INPUT...\n"
            "Creates an optionally-signed Microsoft APPX package.\n"
            "\n"
            "Options:\n"
            "  -c pfx-file   sign the APPX with the private key file\n"
            "  -h            show this usage text and exit\n"
            "  -o appx-file  write the APPX to the file (required)\n"
            "  -0, -1, -2, -3, -4, -5, -6, -7, -8, -9\n"
            "                ZIP compression level\n"
            "  -0            no ZIP compression (store files)\n"
            "  -9            best ZIP compression\n"
            "\n"
            "An input is either:\n"
            "  A directory, indicating that all files and subdirectories \n"
            "    of that directory are included in the package, or\n"
            "  A file name, indicating that the file is included in the \n"
            "    root of the package.\n"
            "\n"
            "Supported target systems:\n"
            "  Windows 10 (UAP)\n"
            "  Windows 10 Mobile\n",
            programName);
}
}

int main(int argc, char **argv) try {
    const char *programName = argv[0];
    const char *certPath = NULL;
    const char *appxPath = NULL;
    int compressionLevel = Z_NO_COMPRESSION;
    while (int c = getopt(argc, argv, "0123456789c:ho:")) {
        if (c == -1) {
            break;
        }
        switch (c) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                compressionLevel = c - '0';
                break;
            case 'c':
                certPath = optarg;
                break;
            case 'o':
                appxPath = optarg;
                break;
            case '?':
                fprintf(stderr, "Unknown option: %c\n", optopt);
                PrintUsage(programName);
                return 1;
            case 'h':
                PrintUsage(programName);
                return 0;
        }
    }
    if (!appxPath) {
        fprintf(stderr, "Missing -o\n");
        PrintUsage(programName);
        return 1;
    }
    argc -= optind;
    argv += optind;
    if (argc <= 0) {
        fprintf(stderr, "Missing inputs\n");
        PrintUsage(programName);
        return 1;
    }
    std::unordered_map<std::string, std::string> fileNames;
    for (char *const *i = argv; i != argv + argc; ++i) {
        const char *arg = *i;
        const char *equalSeparator = strchr(arg, '=');
        if (equalSeparator) {
            // ArchivePath=LocalPath specified.
            fileNames.insert(std::make_pair(std::string(arg, equalSeparator),
                                            std::string(equalSeparator + 1)));
        } else {
            // Local path specified. Infer archive path.
            GetArchiveFileList(arg, fileNames);
        }
    }
    std::string certPathString = certPath ?: "";
    FilePtr appx = Open(appxPath, "wb");
    WriteAppx(appx, fileNames, certPath ? &certPathString : nullptr,
              compressionLevel);
    return 0;
} catch (std::exception &e) {
    fprintf(stderr, "%s\n", e.what());
    return 1;
}
