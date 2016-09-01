# fb-util-for-appx

`appx` is a tool which creates and optionally signs
Microsoft Windows APPX packages.

## Supported targets

`appx` can create APPX packages for the following operating
systems:

* Windows 10 (UAP)
* Windows 10 Mobile

## Building appx

Prerequisites:

* CMake >= 3.0
* OpenSSL developer library
* zlib developer library

Supported build and host platforms and toolchains:

* GNU/Linux with GCC 4.8.1 or GCC 4.9
* Mac OS X 10.10.5 with clang 3.6.0

Build:

    mkdir Build && cd Build && cmake .. && make

On OS X, you'll need to explicitly point cmake to your OpenSSL
installation. The easiest method is to install OpenSSL using
[Homebrew](http://brew.sh/) and then pass `-DOPENSSL_ROOT_DIR=$(brew --prefix openssl)`
when invoking cmake. You can also compile OpenSSL [from source](https://github.com/openssl/openssl)
and set `OPENSSL_ROOT_DIR` accordingly.

Install:

    cd Build && make install

## Running appx

Run `appx -h` for usage information.

## Contributing

fb-util-for-appx actively welcomes contributions from the community.
If you're interested in contributing, be sure to check out the
[contributing guide](https://github.com/facebook/fb-util-for-appx/blob/master/CONTRIBUTING.md).
It includes some tips for getting started in the codebase, as well
as important information about the code of conduct, license, and CLA.
