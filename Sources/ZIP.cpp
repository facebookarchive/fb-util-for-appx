//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//

#include <APPX/ZIP.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace facebook {
namespace appx {
    std::string ZIPFileEntry::SanitizedFileName(const std::string &fileName)
    {
        static const char kWhitelist[] =
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789"
            ".[]() _-/";
        std::string s;
        s.reserve(fileName.size());
        for (char c : fileName) {
            if (std::strchr(kWhitelist, c)) {
                s += c;
            } else {
                char buffer[5];
                int rc = std::snprintf(buffer, sizeof(buffer), "%%%02X", c);
                assert(rc == 3);
                s += buffer;
            }
        }
        return s;
    }
}
}
