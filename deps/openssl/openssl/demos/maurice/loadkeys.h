/* NOCW */
/*
        Please read the README file for condition of use, before
        using this software.

        Maurice Gittens  <mgittens@gits.nl>   January 1997

*/

#ifndef LOADKEYS_H_SEEN
#define LOADKEYS_H_SEEN

#include <openssl/evp.h>

EVP_PKEY * ReadPublicKey(const char *certfile);
EVP_PKEY *ReadPrivateKey(const char *keyfile);

#endif

