OpenSSL Demonstration Applications

This folder contains source code that demonstrates the proper use of the OpenSSL
library API.

bio:                   Demonstration of a simple TLS client and server

certs:                 Demonstration of creating certs, using OCSP

cipher:
aesgcm.c               Demonstration of symmetric cipher GCM mode encrypt/decrypt
aesccm.c               Demonstration of symmetric cipher CCM mode encrypt/decrypt

cms:

digest:
EVP_MD_demo.c          Compute a digest from multiple buffers
EVP_MD_stdin.c         Compute a digest with data read from stdin
EVP_f_md.c             Compute a digest using BIO and EVP_f_md

kdf:
hkdf.c                 Demonstration of HMAC based key derivation
pbkdf2.c               Demonstration of PBKDF2 password based key derivation
scrypt.c               Demonstration of SCRYPT password based key derivation

mac:
gmac.c                 Demonstration of GMAC message authentication

pkey:
EVP_PKEY_EC_keygen.c   Generate an EC key.

smime:

pkcs12:
pkread.c               Print out a description of a PKCS12 file.
pkwrite.c              Add a password to an existing PKCS12 file.

signature:
EVP_Signature_demo.c   Compute and verify a signature from multiple buffers
