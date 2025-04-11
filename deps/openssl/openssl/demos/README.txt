OpenSSL Demonstration Applications

This folder contains source code that demonstrates the proper use of the OpenSSL
library API.

Note: Makefiles are provided in the demo subfolders to demonstrate how to build
them, but are not frequently used.  Configure openssl with enable-demos to build
them automatically through the perl based build system

bio:                   Demonstration of a simple TLS client and server

certs:                 Demonstration of creating certs, using OCSP

cipher:
aesgcm.c               Demonstration of symmetric cipher GCM mode encrypt/decrypt
aesccm.c               Demonstration of symmetric cipher CCM mode encrypt/decrypt
ariacbc.c              Demonstration of symmetric cipher CBC mode encrypt/decrypt

cms:                   Demonstrations related to the Cryptographic Message
                       Syntax (CMS) standard

digest:
EVP_MD_demo.c          Compute a digest from multiple buffers
EVP_MD_stdin.c         Compute a digest with data read from stdin
EVP_MD_xof.c           Compute a digest using the SHAKE256 XOF
EVP_f_md.c             Compute a digest using BIO and EVP_f_md

encode:
rsa_encode.c           Encode and decode PEM-encoded RSA keys

encrypt:
rsa_encrypt.c          Encrypt and decrypt data using an RSA keypair.

guide:                   Sample code from the OpenSSL Guide tutorials. See
                         the oss-guide-quic-introduction(7) man page.
quic-client-block.c:     A simple blocking QUIC client
quic-client-non-block.c: A simple non-blocking QUIC client
quic-multi-stream.c:     A simple QUIC client using multiple streams
tls-client-block.c:      A simple blocking SSL/TLS client
tls-client-non-block.c:  A simple non-blocking SSL/TLS client

http3:                 Demonstration of how to use OpenSSL's QUIC capabilities
                       for HTTP/3.

kdf:
hkdf.c                 Demonstration of HMAC based key derivation
pbkdf2.c               Demonstration of PBKDF2 password based key derivation
scrypt.c               Demonstration of SCRYPT password based key derivation

keyexch:
x25519.c               Demonstration of X25519 based key exchange
ecdh.c                 Demonstration of ECDH key exchange

mac:
gmac.c                 Demonstration of GMAC message authentication
poly1305.c             Demonstration of Poly1305-AES message authentication
siphash.c              Demonstration of SIPHASH message authentication

pkcs12:
pkread.c               Print out a description of a PKCS12 file.
pkwrite.c              Add a password to an existing PKCS12 file.

pkey:
EVP_PKEY_EC_keygen.c           Generate an EC key.
EVP_PKEY_RSA_keygen.c          Generate an RSA key.
EVP_PKEY_DSA_keygen.c          Generate a DSA key.
EVP_PKEY_DSA_paramgen.c        Generate a DSA param key.
EVP_PKEY_DSA_paramvalidate.c   Validate a DSA param key.
EVP_PKEY_DSA_paramfromdata.c   Load a DSA param key using raw data.

signature:
EVP_EC_Signature_demo.c   Compute and verify an EC signature.
EVP_DSA_Signature_demo.c  Compute and verify a DSA signature.
EVP_ED_Signature_demo.c   Compute and verify an ED25519 signature.
rsa_pss_direct.c          Compute and verify an RSA-PSS signature from a hash
rsa_pss_hash.c            Compute and verify an RSA-PSS signature over a buffer

smime:                 Demonstrations related to S/MIME

sslecho:
main.c                 Simple SSL/TLS echo client/server.
