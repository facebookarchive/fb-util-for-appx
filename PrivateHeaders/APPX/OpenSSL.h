//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <memory>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/pkcs12.h>
#include <stdexcept>
#include <string>

namespace facebook {
namespace appx {
    // An exception representing an OpenSSL error.
    class OpenSSLException : public std::runtime_error
    {
    public:
        // Uses the last OpenSSL error for this thread.
        OpenSSLException();
        explicit OpenSSLException(unsigned long error);
        // Uses the last OpenSSL error for this thread.
        explicit OpenSSLException(const std::string &message);
        OpenSSLException(const std::string &message, unsigned long error);
    };

    template <typename T, void (*Deleter)(T *)>
    class OpenSSLPtrHelper
    {
    private:
        struct DeleterWrapper
        {
            void operator()(T *p)
            {
                if (p) {
                    Deleter(p);
                }
            }
        };

    public:
        using Type = std::unique_ptr<T, DeleterWrapper>;
    };

    // A smart pointer for OpenSSL types.
    template <typename T, void (*Deleter)(T *)>
    using OpenSSLPtr = typename OpenSSLPtrHelper<T, Deleter>::Type;

    // Smart pointers for common OpenSSL types.
    using ASN1_STRINGPtr = OpenSSLPtr<ASN1_STRING, ASN1_STRING_free>;
    using ASN1_TYPEPtr = OpenSSLPtr<ASN1_TYPE, ASN1_TYPE_free>;
    using BIOPtr = OpenSSLPtr<BIO, BIO_free_all>;
    using PKCS12Ptr = OpenSSLPtr<PKCS12, PKCS12_free>;
}
}
