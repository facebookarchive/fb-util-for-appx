//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//

#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace facebook {
namespace appx {
    template <typename TTarget>
    struct RangeChecker
    {
        // Ensures the given TTarget (integral type) can fit inside TSource
        // (integral type). If not, std::range_error is thrown.
        template <typename TSource>
        static TTarget Check(TSource x)
        {
            static_assert(std::is_integral<TSource>::value ||
                              std::is_enum<TSource>::value,
                          "TSource must be integral");
            static_assert(std::is_integral<TTarget>::value,
                          "TTarget must be integral");
            if (x > std::numeric_limits<TTarget>::max()) {
                throw std::range_error("Number out of range");
            }
            if (x < std::numeric_limits<TTarget>::min()) {
                throw std::range_error("Number out of range");
            }
            return static_cast<TTarget>(x);
        }
    };
}
}

// Expands to a list of std::uint8_t expressions, suitable for embedding in an
// array literal.
#define FB_BYTES_1(x) ::facebook::appx::RangeChecker<::std::uint8_t>::Check((x))

#define FB_BYTES_2_LE(x)                                              \
    static_cast<::std::uint8_t>(                                      \
        ::facebook::appx::RangeChecker<::std::uint16_t>::Check((x))), \
        static_cast<::std::uint8_t>(static_cast<::std::uint16_t>((x)) >> 8)

#define FB_BYTES_4_LE(x)                                                      \
    static_cast<::std::uint8_t>(                                              \
        ::facebook::appx::RangeChecker<::std::uint32_t>::Check((x))),         \
        static_cast<::std::uint8_t>(static_cast<::std::uint32_t>((x)) >> 8),  \
        static_cast<::std::uint8_t>(static_cast<::std::uint32_t>((x)) >> 16), \
        static_cast<::std::uint8_t>(static_cast<::std::uint32_t>((x)) >> 24)

#define FB_BYTES_8_LE(x)                                                      \
    static_cast<::std::uint8_t>(                                              \
        ::facebook::appx::RangeChecker<::std::uint64_t>::Check((x))),         \
        static_cast<::std::uint8_t>(static_cast<::std::uint64_t>((x)) >> 8),  \
        static_cast<::std::uint8_t>(static_cast<::std::uint64_t>((x)) >> 16), \
        static_cast<::std::uint8_t>(static_cast<::std::uint64_t>((x)) >> 24), \
        static_cast<::std::uint8_t>(static_cast<::std::uint64_t>((x)) >> 32), \
        static_cast<::std::uint8_t>(static_cast<::std::uint64_t>((x)) >> 40), \
        static_cast<::std::uint8_t>(static_cast<::std::uint64_t>((x)) >> 48), \
        static_cast<::std::uint8_t>(static_cast<::std::uint64_t>((x)) >> 56)
