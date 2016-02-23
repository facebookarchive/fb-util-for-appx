//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//

#include <APPX/XML.h>
#include <stdexcept>
#include <string>

namespace facebook {
namespace appx {
    std::string XMLEncodeString(const std::string &s)
    {
        // TODO(strager): Escape instead of raising an error.
        static const char kWhitelist[] =
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789"
            ".[]() _-+\\%";
        std::string::size_type pos = s.find_first_not_of(kWhitelist);
        if (pos != std::string::npos) {
            throw std::runtime_error(
                std::string("String contains unsupported character '") +
                s[pos] + "': " + s);
        }
        return s;
    }
}
}
