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

The `-DSTATIC_BUILD=ON` option is not supported on macOS. To create a static (statically links zlib and openssl) `appx` executable on macOS the following cmake command can be used:

```bash
cmake .. -DSTATIC_BUILD=OFF -DCMAKE_BUILD_TYPE=RELEASE -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) -DOPENSSL_CRYPTO_LIBRARY=/usr/local/opt/openssl/lib/libcrypto.a -DOPENSSL_SSL_LIBRARY=/usr/local/opt/openssl/lib/libssl.a  -DZLIB_LIBRARY=/usr/local/opt/zlib/lib/libz.a
```
To test whether the static libraries have been used, the `otool -L appx` command should give the following output (versions may vary):

```bash
$> otool -L appx
appx:
	/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1252.0.0)
	/usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 400.9.0)
$>
```

On Linux it is also possible to statically link zlib and openssl by manually specifying the library locations:

```bash
cmake .. -DSTATIC_BUILD=OFF -DCMAKE_BUILD_TYPE=RELEASE -DOPENSSL_CRYPTO_LIBRARY=/usr/lib/x86_64-linux-gnu/libcrypto.a -DOPENSSL_SSL_LIBRARY=/usr/lib/x86_64-linux-gnu/libssl.a -DZLIB_LIBRARY=/usr/lib/x86_64-linux-gnu/libz.a
```
To test the shared library dependencies use the ldd command:

```bash
$> ldd appx
	linux-vdso.so.1 =>  (0x00007ffc2d384000)
	libdl.so.2 => /lib/x86_64-linux-gnu/libdl.so.2 (0x00007f938c41e000)
	libstdc++.so.6 => /usr/lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007f938c09c000)
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f938bd92000)
	libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007f938bb7c000)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f938b7b2000)
	/lib64/ld-linux-x86-64.so.2 (0x0000556a9363a000)
$>
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
