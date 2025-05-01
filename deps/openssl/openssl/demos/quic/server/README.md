Simple single-connection QUIC server example
============================================

This is a simple example of a QUIC server that accepts and handles one
connection at a time. It demonstrates blocking use of the QUIC server API.

Type `make` to build and `make run` to run.

Usage:

```bash
./server <port-number> <certificate-file> <key-file>
```

Example client usage:

```bash
openssl s_client -quic -alpn ossltest -connect 127.0.0.1:<port-number>
```
