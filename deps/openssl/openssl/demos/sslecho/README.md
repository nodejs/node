OpenSSL Simple Echo Client/Server
=================================

This project implements a simple echo client/server.

It is a console application, with command line parameters determining the mode
of operation (client or server). Start it with no parameters to see usage.

The server code was adapted from the Simple TLS Server on the OpenSSL Wiki.
The server code was modified to perform the echo function, and client code
was added to open a connection with the server and to send keyboard input
to the server.

The new client code illustrates that:

- Connection to the SSL server starts as a standard TCP 'connect'.
- Once connected with TCP, the client 'upgrades' to SSL using
  SSL_connect().
- When the SSL connection completes, data is sent and received using
  SSL_write() and SSL_read().
- Pretty simple.

The cert.pem and key.pem files included are self signed certificates with the
"Common Name" of 'localhost'.

Best to create the 'pem' files using an actual hostname.
