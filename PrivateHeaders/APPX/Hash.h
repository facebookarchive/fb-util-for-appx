//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <cstdint>
#include <cstring>
#include <openssl/sha.h>

namespace facebook {
namespace appx {
    struct SHA256Hash
    {
        explicit SHA256Hash()
        {
            std::memset(bytes, 0, sizeof(this->bytes));
        }

        explicit SHA256Hash(const std::uint8_t *bytes)
        {
            std::memcpy(this->bytes, bytes, sizeof(this->bytes));
        }

        // Hashes the input bytes, returning the digest.
        static SHA256Hash DigestFromBytes(std::size_t size,
                                          const std::uint8_t *bytes)
        {
            std::uint8_t hash[sizeof(SHA256Hash::bytes)];
            ::SHA256(bytes, size, hash);
            return SHA256Hash(hash);
        }

        std::uint8_t bytes[SHA256_DIGEST_LENGTH];
    };
}
}
