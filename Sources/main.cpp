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
#include <fstream>
#include <fts.h>
#include <iostream>
#include <memory>
#include <sstream>
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

class MalformedMappingFileError : public std::exception
{
public:
    explicit MalformedMappingFileError(off_t lineNumber)
        : lineNumber(lineNumber)
    {
        this->SetFileName(nullptr);
    }

    const char *what() const noexcept override
    {
        return this->message.c_str();
    }

    void SetFileName(const char *fileName)
    {
        if (!fileName || strcmp(fileName, "") == 0) {
            fileName = "(unknown)";
        }
        std::ostringstream ss;
        ss << "Malformed mapping file: " << fileName << ":" << this->lineNumber;
        this->message = ss.str();
    }

private:
    std::string message;
    off_t lineNumber;
};

bool GetLine(std::istream &file, std::string &out, char delimiter)
{
    try {
        std::getline(file, out, delimiter);
    } catch (std::ios_base::failure &e) {
        if (file.fail() || file.eof()) {
            // Handled below.
        } else {
            throw;
        }
    }
    if (file.bad()) {
        return false;
    }
    if (file.eof()) {
        if (file.fail() && out.empty()) {
            // Don't treat starting at EOF as a failure.
            file.clear(file.rdstate() & ~std::istream::failbit);
            return false;
        } else {
            // We parsed something and reached EOF. Process the line.
            return true;
        }
    }
    return true;
}

void GetArchiveFileListFromMappingFile(
    std::istream &mappingFile,
    std::unordered_map<std::string, std::string> &fileNames)
{
    static const char kWhitespace[] = " \t";
    // TODO(strager): Make this parser more accepting. This parser is way too
    // strict.
    bool didReadHeader = false;
    off_t lineNumber = 1;
    for (;;) {
        std::string line;
        if (!GetLine(mappingFile, line, '\n')) {
            break;
        }
        if (mappingFile.fail()) {
            // The line is too long.
            throw MalformedMappingFileError(lineNumber);
        }

        // Trim leading and trailing whitespace and ignore blank lines.
        {
            auto first = line.find_first_not_of(kWhitespace);
            if (first == std::string::npos) {
                // Blank line.
                continue;
            }
            auto last = line.find_last_not_of(kWhitespace);
            assert(last != std::string::npos);
            line.erase(last + 1, std::string::npos);
            line.erase(0, first);
        }
        if (didReadHeader) {
            // Parse the following:
            //
            //     "localPath" "archiveName"
            //
            // TODO(strager): Parse escaped quotes and other characters.
            std::string::size_type quote1 = 0;
            if (line[quote1] != '"') {
                // Garbage before the first quote.
                throw MalformedMappingFileError(lineNumber);
            }
            auto quote2 = line.find('"', quote1 + 1);
            if (quote2 == std::string::npos) {
                // Missing the second quote.
                throw MalformedMappingFileError(lineNumber);
            }
            if (quote2 == quote1 + 1) {
                // Empty local path.
                throw MalformedMappingFileError(lineNumber);
            }
            auto quote3 = line.find_first_not_of(kWhitespace, quote2 + 1);
            if (quote3 == std::string::npos) {
                // Missing the archive name.
                throw MalformedMappingFileError(lineNumber);
            }
            if (line[quote3] != '"') {
                // Garbage between the second and third quotes.
                throw MalformedMappingFileError(lineNumber);
            }
            auto quote4 = line.find('"', quote3 + 1);
            if (quote4 == std::string::npos) {
                // Missing the fourth quote.
                throw MalformedMappingFileError(lineNumber);
            }
            if (quote4 == quote3 + 1) {
                // Empty archive path.
                throw MalformedMappingFileError(lineNumber);
            }
            if (quote4 != line.size() - 1) {
                // Garbage after the fourth quote.
                throw MalformedMappingFileError(lineNumber);
            }
            std::string localPath =
                line.substr(quote1 + 1, quote2 - quote1 - 1);
            std::string archiveName =
                line.substr(quote3 + 1, quote4 - quote3 - 1);
            fileNames.emplace(std::move(archiveName), std::move(localPath));
        } else {
            if (line != "[Files]") {
                throw MalformedMappingFileError(lineNumber);
            }
            didReadHeader = true;
        }
        lineNumber += 1;
    }
}

void PrintUsage(const char *programName)
{
    fprintf(stderr,
            "Usage: %s -o APPX [OPTION]... INPUT...\n"
            "Creates an optionally-signed Microsoft APPX or APPXBUNDLE package.\n"
            "\n"
                "Options:\n"
            "  -c pfx-file     sign the APPX with the private key file\n"
            "  -m module-file  an opensc module to use for signing\n"
            "  -s slot         a smartcart slot id\n"
            "  -k key-id       a smartcard key id\n"
            "  -f map-file     specify inputs from a mapping file\n"
            "  -f -            specify a mapping file through standard input\n"
            "  -h              show this usage text and exit\n"
            "  -b              produce APPXBUNDLE instead of APPX\n"
            "  -o output-file  write the APPX (or APPXBUNDLE if -b is specified)\n"
            "                  to the output-file (required)\n"
            "  -0, -1, -2, -3, -4, -5, -6, -7, -8, -9\n"
            "                  ZIP compression level\n"
            "  -0              no ZIP compression (store files)\n"
            "  -9              best ZIP compression\n"
            "\n"
            "An input is either:\n"
            "  A directory, indicating that all files and subdirectories \n"
            "    of that directory are included in the package, or\n"
            "  A file name, indicating that the file is included in the \n"
            "    root of the package, or\n"
            "  A mapping file specified with the -f option.\n"
            "\n"
            "A mapping file has the following form:\n"
            "\n"
            "  [Files]\n"
            "  \"/path/to/local/file.exe\" \"appx_file.exe\"\n"
            "\n"
            "Signing through a smartcard can be achieved as such:\n"
            "-m /usr/lib/x86_64-linux-gnu/opensc-pkcs11.so -s 1 -k 0 -p passphrase\n"
            "If no passphrase is provided, APPX_PIV_PIN environment variable will be used\n"
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
    const char *modulePath = NULL;
    const char *pivPin = NULL;
    int slotId = -1;
    int keyId = -1;
    int compressionLevel = Z_NO_COMPRESSION;
    bool isBundle = false;
    std::unordered_map<std::string, std::string> fileNames;
    while (int c = getopt(argc, argv, "0123456789bc:f:ho:m:s:k:p:")) {
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
            case 'b':
                isBundle = true;
                break;
            case 'c':
                certPath = optarg;
                break;
            case 'f':
                if (strcmp(optarg, "-") == 0) {
                    std::cin.exceptions(std::istream::badbit |
                                        std::istream::failbit);
                    GetArchiveFileListFromMappingFile(std::cin, fileNames);
                } else {
                    std::ifstream file;
                    file.exceptions(std::ifstream::badbit |
                                    std::ifstream::failbit);
                    file.open(optarg);
                    try {
                        GetArchiveFileListFromMappingFile(file, fileNames);
                    } catch (MalformedMappingFileError &e) {
                        e.SetFileName(optarg);
                        throw;
                    }
                }
                break;
            case 'o':
                appxPath = optarg;
                break;
            case 'm':
                modulePath = optarg;
                break;
            case 's':
                slotId = atoi(optarg);
                break;
            case 'k':
                keyId = atoi(optarg);
                break;
            case 'p':
                pivPin = optarg;
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
    if (modulePath != nullptr && certPath != nullptr) {
        fprintf(stderr, "Incompatible -c & -m options provided\n");
        return 1;
    }
    if (modulePath != nullptr) {
        if (slotId == -1) {
            fprintf(stderr, "Missing -s parameter for smartcard signing\n");
            return 1;
        }
        if (slotId < 0) {
            fprintf(stderr, "Invalid value provided for -s parameter: %d\n", slotId);
            return 1;
        }
        if (keyId == -1) {
            fprintf(stderr, "Missing -k parameter for smartcard signing\n");
            return 1;
        }
        if (keyId < 0 || keyId > UINT8_MAX) {
            fprintf(stderr, "Invalid value provided for -k parameter: %d\n", keyId);
            return 1;
        }
        if (pivPin == nullptr) {
            pivPin = getenv("APPX_PIV_PIN");
            if (pivPin == nullptr) {
                fprintf(stderr, "No PIV passphrase provided\n");
                return 1;
            }
        }
    }
    argc -= optind;
    argv += optind;
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
    if (fileNames.empty()) {
        fprintf(stderr, "Missing inputs\n");
        PrintUsage(programName);
        return 1;
    }
    if (isBundle && fileNames.count("AppxMetadata/AppxBundleManifest.xml") == 0) {
        fprintf(stderr, "You need to provide AppxBundleManifest.xml!\n");
        return 1;
    }
    FilePtr appx = Open(appxPath, "wb");
    if (certPath != nullptr || modulePath != nullptr )
    {
        facebook::appx::SigningParams signingParams;
        if (certPath != nullptr)
            signingParams = std::make_tuple(certPath, "", 0u, 0u, "");
        else
            signingParams = std::make_tuple("", modulePath, slotId, keyId, pivPin);
        WriteAppx(appx, fileNames, &signingParams, compressionLevel, isBundle);
    }
    else
    {
        WriteAppx(appx, fileNames, nullptr, compressionLevel, isBundle);
    }

    return 0;
} catch (std::exception &e) {
    fprintf(stderr, "%s\n", e.what());
    return 1;
}
