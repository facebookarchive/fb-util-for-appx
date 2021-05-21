//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <APPX/OpenSSL.h>
#include <openssl/err.h>

namespace facebook {
namespace appx {
    namespace {
        static std::string GetErrorString(unsigned long error)
        {
            // Make sure human-friendly strings are used.
            ERR_load_crypto_strings();

            char buffer[256];
            static_assert(sizeof(buffer) >= 120,
                          "Buffer must be as least as big as required "
                          "according to OpenSSL documentation");
            ERR_error_string_n(error, buffer, sizeof(buffer));
            return buffer;
        }
    }

    OpenSSLException::OpenSSLException() : OpenSSLException(ERR_peek_error())
    {
    }

    OpenSSLException::OpenSSLException(unsigned long error)
        : std::runtime_error(GetErrorString(error))
    {
    }

    OpenSSLException::OpenSSLException(const std::string &message)
        : OpenSSLException(message, ERR_peek_error())
    {
    }

    OpenSSLException::OpenSSLException(const std::string &message,
                                       unsigned long error)
        : std::runtime_error(GetErrorString(error) + ": " + message)
    {
    }
}
}
