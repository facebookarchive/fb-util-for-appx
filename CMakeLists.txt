#
# Copyright (c) 2016-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.
#

cmake_minimum_required(VERSION 3.0)
enable_testing()

project(appx)

include(CheckCXXSourceCompiles)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(appx
               Sources/APPX.cpp
               Sources/File.cpp
               Sources/OpenSSL.cpp
               Sources/Sign.cpp
               Sources/XML.cpp
               Sources/ZIP.cpp
               Sources/main.cpp)
target_include_directories(appx
                           PRIVATE
                           PrivateHeaders
                           ${OPENSSL_INCLUDE_DIR}
                           ${ZLIB_INCLUDE_DIRS})
target_link_libraries(appx
                      PRIVATE
                      ${OPENSSL_LIBRARIES}
                      ${ZLIB_LIBRARIES})
install(TARGETS appx RUNTIME DESTINATION bin)

# Check for C++11 support.
function (APPX_CHECK_TEMPLATE_USING_WITH_FLAGS NAME FLAGS)
  set(CMAKE_REQUIRED_FLAGS "${FLAGS}")
  check_cxx_source_compiles("template<typename T>
                             using ID = T;

                             ID<int>
                             main()
                             {
                               return 0;
                             }
                             "
                             "${NAME}"
                             FAIL_REGEX "C\\\\+\\\\+11 extension")
endfunction ()
function (APPX_CXX_STD_FLAGS OUT_VAR)
  appx_check_template_using_with_flags(APPX_HAS_TEMPLATE_USING "")
  if (APPX_HAS_TEMPLATE_USING)
    set("${OUT_VAR}" "" PARENT_SCOPE)
    return ()
  endif ()
  appx_check_template_using_with_flags(APPX_HAS_TEMPLATE_USING_STD_CXX11
                                       "-std=c++11")
  if (APPX_HAS_TEMPLATE_USING_STD_CXX11)
    set("${OUT_VAR}" "-std=c++11" PARENT_SCOPE)
    return ()
  endif ()
  message(FATAL_ERROR "Compiler lacks C++11 support")
endfunction ()
appx_cxx_std_flags(APPX_CXX_STD_FLAGS)
set_property(TARGET appx
             APPEND PROPERTY COMPILE_OPTIONS "${APPX_CXX_STD_FLAGS}")

# OpenSSL is deprecated on OS X.
function (APPX_CHECK_OPENSSL_WITH_FLAGS NAME FLAGS)
  set(CMAKE_REQUIRED_FLAGS "${FLAGS}")
  set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${OPENSSL_LIBRARIES})
  check_cxx_source_compiles("#include <openssl/asn1.h>

                             int
                             main()
                             {
                               ASN1_STRING *s = ASN1_STRING_new();
                               ASN1_STRING_free(s);
                               return 0;
                             }
                             "
                             "${NAME}"
                             FAIL_REGEX "is deprecated")
endfunction ()
appx_check_openssl_with_flags(APPX_HAS_UNDEPRECATED_OPENSSL "")
if (NOT APPX_HAS_UNDEPRECATED_OPENSSL)
  appx_check_openssl_with_flags(APPX_HAS_SUPPRESSABLE_DEPRECATED_OPENSSL
                                -Wno-deprecated-declarations)
  if (APPX_HAS_SUPPRESSABLE_DEPRECATED_OPENSSL)
    set_property(TARGET appx
                 APPEND PROPERTY COMPILE_OPTIONS -Wno-deprecated-declarations)
  endif ()
endif ()

function (APPX_ADD_TEST NAME)
  add_test(NAME "${NAME}"
           COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/Tests/${NAME}.py")
  set_property(TEST "${NAME}" PROPERTY
               ENVIRONMENT "FB_APPX_PATH=$<TARGET_FILE:appx>")
endfunction ()
appx_add_test(TestInputs)
appx_add_test(TestValidZIP)
appx_add_test(TestXMLEscaping)
appx_add_test(TestZIPEscaping)
appx_add_test(TestContentTypes)
appx_add_test(TestEmptyFile)
