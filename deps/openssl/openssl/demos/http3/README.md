HTTP/3 Demo using OpenSSL QUIC and nghttp3
==========================================

This is a simple demo of how to use HTTP/3 with OpenSSL QUIC using the HTTP/3
library “[nghttp3](https://github.com/ngtcp2/nghttp3)”.

The demo is structured into two parts:

- an adaptation layer which binds nghttp3 to OpenSSL's QUIC implementation
  (`ossl-nghttp3.c`);
- a simple application which makes an HTTP/3 request using this adaptation
  layer (`ossl-nghttp3-demo.c`).

The Makefile in this directory can be used to build the demo on \*nix-style
systems.  You will need the `nghttp3` library and header file.  On
Ubuntu, these can be obtained by installing the package `libnghttp3-dev`.

Running the Demo
----------------

Depending on your system configuration it may be necessary to set the
`SSL_CERT_FILE` or `SSL_CERT_DIR` environment variables to a location where
trusted root CA certificates can be found.

After building by running `make`, run `./ossl-nghttp3-demo` with a hostname and
port as the sole argument:

```shell
$ make
$ LD_LIBRARY_PATH=../.. ./ossl-nghttp3-demo www.google.com:443
```

The demo produces the HTTP response headers in textual form as output followed
by the response body.

See Also
--------

- [nghttp3](https://github.com/ngtcp2/nghttp3)
