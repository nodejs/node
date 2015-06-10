# Building io.js with FIPS-compliant OpenSSL

It is possible to build io.js with [OpenSSL FIPS module][0].

Instructions:

1. Download and verify `openssl-fips-x.x.x.tar.gz` from
   https://www.openssl.org/source/
2. Extract source to `openssl-fips` folder
3. ``cd openssl-fips && ./config fipscanisterbuild --prefix=`pwd`/out``
   (NOTE: On OS X, you may want to run
    ``./Configure darwin64-x86_64-cc --prefix=`pwd`/out`` if you are going to
    build x64-mode io.js)
4. `make -j && make install`
5. Get into io.js checkout folder
6. `./configure --openssl-fips=/path/to/openssl-fips/out`
7. Build io.js with `make -j`
8. Verify with `node -p "process.versions.openssl"` (`1.0.2a-fips`)

NOTE: Windows is not yet supported

[0]: https://www.openssl.org/docs/fips/fipsnotes.html
