The OpenSSL Guide Demos
=======================

The demos in this directory are the complete source code for the applications
developed in the OpenSSL Guide tutorials. Refer to the various tutorial pages in
the [guide] for an extensive discussion on the demos available here.

They must be built before they can be run. An example UNIX style Makefile is
supplied. Just type "make" from this directory on a Linux/UNIX system.

Running the TLS Demos
---------------------

To run the demos when linked with a shared library (default) ensure that
libcrypto and libssl are on the library path. For example, assuming you have
already built OpenSSL from this source and in the default location then to run
the tls-client-block demo do this:

LD_LIBRARY_PATH=../.. ./tls-client-block hostname port

In the above replace "hostname" and "port" with the hostname and the port number
of the server you are connecting to.

The above assumes that your default trusted certificate store containing trusted
CA certificates has been properly setup and configured as described on the
[TLS Introduction] page.

You can run a test server to try out these demos using the "openssl s_server"
command line utility and using the test server certificate and key provided in
this directory. For example:

LD_LIBRARY_PATH=../.. ../../apps/openssl s_server -www -accept localhost:4443 -cert servercert.pem -key serverkey.pem

The test server certificate in this directory will use a CA that will not be in
your default trusted certificate store. The CA certificate to use is also
available in this directory. To use it you can override the default trusted
certificate store like this:

SSL_CERT_FILE=rootcert.pem LD_LIBRARY_PATH=../.. ./tls-client-block localhost 4443

If the above command is successful it will connect to the test "s_server" and
send a simple HTTP request to it. The server will respond with a page of
information giving details about the TLS connection that was used.

Note that the test server certificate used here is only suitable for use on
"localhost".

The tls-client-non-block demo can be run in exactly the same way. Just replace
"tls-client-block" in the above example commands with "tls-client-non-block".

Running the QUIC Demos
----------------------

The QUIC demos can be run in a very similar way to the TLS demos.

While in the demos directory the QUIC server can be run like this:

LD_LIBRARY_PATH=../.. ./quic-server-block 4443 ./chain.pem ./pkey.pem

The QUIC demos can then be run in the same was as the TLS demos. For example
to run the quic-client-block demo:

SSL_CERT_FILE=chain.pem LD_LIBRARY_PATH=../.. ./quic-client-block localhost 4443

Notes on the quic-hq-interop demo
---------------------------------

The quic-hq-interop demo is effectively the same as the quic-client-nonblock
demo, but is specifically constructed to use the hq-interop alpn for the
purposes of interacting with other demonstration containers found in the
QUIC working group [interop runner](https://github.com/quic-interop/quic-interop-runner)
It is run as follows:

SSL_CERT_FILE=ca.pem LD_LIBRARY_PATH=../../ ./quic-hq-interop host port file

The demo will then do the following:

1. Connect to the server at host/port
2. Negotiates the hq-interop alpn
3. Issues an HTTP 1.0 GET request of the form "GET /$FILE"
3. Reads any response from the server and write it verbatim to stdout

This demo can be used for any hq-interop negotiating server, but its use can
most easily be seen in action in our quic interop container, buildable from
./test/quic_interop_openssl in this source tree.

<!-- Links  -->

[guide]: https://www.openssl.org/docs/manmaster/man7/ossl-guide-introduction.html
[TLS Introduction]: https://www.openssl.org/docs/manmaster/man7/ossl-guide-tls-introduction.html
