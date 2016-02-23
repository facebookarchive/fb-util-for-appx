//
// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//

#include <APPX/File.h>

namespace facebook {
namespace appx {
    ErrnoException::ErrnoException() : ErrnoException(errno)
    {
    }

    ErrnoException::ErrnoException(const std::string &message)
        : ErrnoException(message, errno)
    {
    }

    ErrnoException::ErrnoException(int error)
        :  // Avoid allocations by using the const char * overload of
          // std::runtime_error.
          std::runtime_error(std::strerror(error)),
          error(error)
    {
    }

    ErrnoException::ErrnoException(const std::string &message, int error)
        : std::runtime_error(std::strerror(error) + std::string(": ") +
                             message),
          error(error)
    {
    }
}
}
