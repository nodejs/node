/* crypto/rc4/rc4.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rc4.h>
#include <openssl/evp.h>

char *usage[] = {
    "usage: rc4 args\n",
    "\n",
    " -in arg         - input file - default stdin\n",
    " -out arg        - output file - default stdout\n",
    " -key key        - password\n",
    NULL
};

int main(int argc, char *argv[])
{
    FILE *in = NULL, *out = NULL;
    char *infile = NULL, *outfile = NULL, *keystr = NULL;
    RC4_KEY key;
    char buf[BUFSIZ];
    int badops = 0, i;
    char **pp;
    unsigned char md[MD5_DIGEST_LENGTH];

    argc--;
    argv++;
    while (argc >= 1) {
        if (strcmp(*argv, "-in") == 0) {
            if (--argc < 1)
                goto bad;
            infile = *(++argv);
        } else if (strcmp(*argv, "-out") == 0) {
            if (--argc < 1)
                goto bad;
            outfile = *(++argv);
        } else if (strcmp(*argv, "-key") == 0) {
            if (--argc < 1)
                goto bad;
            keystr = *(++argv);
        } else {
            fprintf(stderr, "unknown option %s\n", *argv);
            badops = 1;
            break;
        }
        argc--;
        argv++;
    }

    if (badops) {
 bad:
        for (pp = usage; (*pp != NULL); pp++)
            fprintf(stderr, "%s", *pp);
        exit(1);
    }

    if (infile == NULL)
        in = stdin;
    else {
        in = fopen(infile, "r");
        if (in == NULL) {
            perror("open");
            exit(1);
        }

    }
    if (outfile == NULL)
        out = stdout;
    else {
        out = fopen(outfile, "w");
        if (out == NULL) {
            perror("open");
            exit(1);
        }
    }

#ifdef OPENSSL_SYS_MSDOS
    /* This should set the file to binary mode. */
    {
# include <fcntl.h>
        setmode(fileno(in), O_BINARY);
        setmode(fileno(out), O_BINARY);
    }
#endif

    if (keystr == NULL) {       /* get key */
        i = EVP_read_pw_string(buf, BUFSIZ, "Enter RC4 password:", 0);
        if (i != 0) {
            OPENSSL_cleanse(buf, BUFSIZ);
            fprintf(stderr, "bad password read\n");
            exit(1);
        }
        keystr = buf;
    }

    EVP_Digest((unsigned char *)keystr, strlen(keystr), md, NULL, EVP_md5(),
               NULL);
    OPENSSL_cleanse(keystr, strlen(keystr));
    RC4_set_key(&key, MD5_DIGEST_LENGTH, md);

    for (;;) {
        i = fread(buf, 1, BUFSIZ, in);
        if (i == 0)
            break;
        if (i < 0) {
            perror("read");
            exit(1);
        }
        RC4(&key, (unsigned int)i, (unsigned char *)buf,
            (unsigned char *)buf);
        i = fwrite(buf, (unsigned int)i, 1, out);
        if (i != 1) {
            perror("write");
            exit(1);
        }
    }
    fclose(out);
    fclose(in);
    exit(0);
    return (1);
}
