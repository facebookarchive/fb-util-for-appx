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

On macOS, you'll need to explicitly point cmake to your OpenSSL
installation. The easiest method is to install OpenSSL using
[Homebrew](http://brew.sh/) and then pass `-DOPENSSL_ROOT_DIR=$(brew --prefix openssl)`
when invoking cmake. You can also compile OpenSSL [from source](https://github.com/openssl/openssl)
and set `OPENSSL_ROOT_DIR` accordingly.

The `-DSTATIC_BUILD=OFF` option is not supported on macOS. To create a static (statically links zlib and openssl) `appx` executable on macOS the following cmake command can be used:

```bash
cmake .. -DSTATIC_BUILD=OFF -DCMAKE_BUILD_TYPE=RELEASE -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) -DOPENSSL_CRYPTO_LIBRARY=/usr/local/opt/openssl/lib/libcrypto.a -DOPENSSL_SSL_LIBRARY=/usr/local/opt/openssl/lib/libssl.a  -DZLIB_LIBRARY=/usr/local/opt/zlib/lib/libz.a
```
To test whether the static libraries have been used, the `otool -L appx` command should give the following output (versions may vary):

```bash
otool -L appx
appx:
	/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1252.0.0)
	/usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 400.9.0)
```

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
