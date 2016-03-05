//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//

#include <APPX/XML.h>
#include <unordered_map>

namespace facebook {
namespace appx {
    std::string XMLEncodeString(const std::string &s)
    {
        static const std::unordered_map<char, const char *> sEncodeMap = {
            {'"', "&quot;"}, {'&', "&amp;"}, {'\'', "&apos;"},
            {'<', "&lt;"},   {'>', "&gt;"},
        };

        std::string encoded;
        encoded.reserve(s.size());
        for (char c : s) {
            auto it = sEncodeMap.find(c);
            if (it != sEncodeMap.end()) {
                encoded += it->second;
            } else {
                encoded += c;
            }
        }
        return encoded;
    }
}
}
