//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <APPX/File.h>
#include <string>
#include <unordered_map>
#include <zlib.h>

namespace facebook {
namespace appx {

    // A tuple containing:
    // - The certificate file if signing using one
    // - The OpenSC module path if signing using a smartcard
    // - The smartcard slot ID containing the signing key
    // - The signing key ID
    // - The PIV PIN to unlock the private key
    // If signing using a certificate file, the last 4 entries will be default
    // initialized
    using SigningParams = std::tuple<std::string, std::string, uint32_t,
                                     uint8_t, std::string>;

    // Creates and optionally signs an APPX file.
    //
    // fileNames maps APPX archive names to local filesystem paths.
    //
    // certParams, if specified, contains the signing parameters, which can be
    // either a path to a certificate file provided as the first string member,
    // or a set of parameters to sign using a smart card, as second to fifth members
    //
    // compressionLevel indicates how much to compress individual files.
    // Z_DEFAULT_COMPRESSION and any value between Z_NO_COMPRESSION and
    // Z_BEST_COMPRESSION are accepted.
    void WriteAppx(
        const FilePtr &zip,
        const std::unordered_map<std::string, std::string> &fileNames,
        const SigningParams *signingParams, int compressionLevel, bool bundle);
}
}
