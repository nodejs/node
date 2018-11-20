/* File : /crypto/engine/vendor_defns/hw_zencod.h */
/* ====================================================================
 * Written by Donnat Frederic (frederic.donnat@zencod.com) from ZENCOD
 * for "zencod" ENGINE integration in OpenSSL project.
 */

#ifndef        _HW_ZENCOD_H_
# define _HW_ZENCOD_H_

# include <stdio.h>

# ifdef  __cplusplus
extern "C" {
# endif                         /* __cplusplus */

# define ZENBRIDGE_MAX_KEYSIZE_RSA       2048
# define ZENBRIDGE_MAX_KEYSIZE_RSA_CRT   1024
# define ZENBRIDGE_MAX_KEYSIZE_DSA_SIGN  1024
# define ZENBRIDGE_MAX_KEYSIZE_DSA_VRFY  1024

/* Library version computation */
# define ZENBRIDGE_VERSION_MAJOR(x)      (((x) >> 16) | 0xff)
# define ZENBRIDGE_VERSION_MINOR(x)      (((x) >>  8) | 0xff)
# define ZENBRIDGE_VERSION_PATCH(x)      (((x) >>  0) | 0xff)
# define ZENBRIDGE_VERSION(x, y, z)              ((x) << 16 | (y) << 8 | (z))

    /*
     * Memory type
     */
    typedef struct zencod_number_s {
        unsigned long len;
        unsigned char *data;
    } zen_nb_t;

# define KEY     zen_nb_t

    /*
     * Misc
     */
    typedef int t_zencod_lib_version(void);
    typedef int t_zencod_hw_version(void);
    typedef int t_zencod_test(void);
    typedef int t_zencod_dump_key(FILE *stream, char *msg, KEY * key);

    /*
     * Key management tools
     */
    typedef KEY *t_zencod_new_number(unsigned long len, unsigned char *data);
    typedef int t_zencod_init_number(KEY * n, unsigned long len,
                                     unsigned char *data);
    typedef unsigned long t_zencod_bytes2bits(unsigned char *n,
                                              unsigned long bytes);
    typedef unsigned long t_zencod_bits2bytes(unsigned long bits);

    /*
     * RSA API
     */
/* Compute modular exponential : y = x**e | n */
    typedef int t_zencod_rsa_mod_exp(KEY * y, KEY * x, KEY * n, KEY * e);
    /*
     * Compute modular exponential : y1 = (x | p)**edp | p, y2 = (x | p)**edp
     * | p, y = y2 + (qinv * (y1 - y2) | p) * q
     */
    typedef int t_zencod_rsa_mod_exp_crt(KEY * y, KEY * x, KEY * p, KEY * q,
                                         KEY * edp, KEY * edq, KEY * qinv);

    /*
     * DSA API
     */
    typedef int t_zencod_dsa_do_sign(unsigned int hash, KEY * data,
                                     KEY * random, KEY * p, KEY * q, KEY * g,
                                     KEY * x, KEY * r, KEY * s);
    typedef int t_zencod_dsa_do_verify(unsigned int hash, KEY * data, KEY * p,
                                       KEY * q, KEY * g, KEY * y, KEY * r,
                                       KEY * s, KEY * v);

    /*
     * DH API
     */
    /* Key generation : compute public value y = g**x | n */
    typedef int t_zencod_dh_generate_key(KEY * y, KEY * x, KEY * g, KEY * n,
                                         int gen_x);
    typedef int t_zencod_dh_compute_key(KEY * k, KEY * y, KEY * x, KEY * n);

    /*
     * RNG API
     */
# define ZENBRIDGE_RNG_DIRECT            0
# define ZENBRIDGE_RNG_SHA1              1
    typedef int t_zencod_rand_bytes(KEY * rand, unsigned int flags);

    /*
     * Math API
     */
    typedef int t_zencod_math_mod_exp(KEY * r, KEY * a, KEY * e, KEY * n);

    /*
     * Symetric API
     */
/* Define a data structure for digests operations */
    typedef struct ZEN_data_st {
        unsigned int HashBufferSize;
        unsigned char *HashBuffer;
    } ZEN_MD_DATA;

    /*
     * Functions for Digest (MD5, SHA1) stuff
     */
    /* output : output data buffer */
    /* input : input data buffer */
    /* algo : hash algorithm, MD5 or SHA1 */
    /*-
     * typedef int t_zencod_hash ( KEY *output, const KEY *input, int algo ) ;
     * typedef int t_zencod_sha_hash ( KEY *output, const KEY *input, int algo ) ;
     */
    /* For now separate this stuff that mad it easier to test */
    typedef int t_zencod_md5_init(ZEN_MD_DATA *data);
    typedef int t_zencod_md5_update(ZEN_MD_DATA *data, const KEY * input);
    typedef int t_zencod_md5_do_final(ZEN_MD_DATA *data, KEY * output);

    typedef int t_zencod_sha1_init(ZEN_MD_DATA *data);
    typedef int t_zencod_sha1_update(ZEN_MD_DATA *data, const KEY * input);
    typedef int t_zencod_sha1_do_final(ZEN_MD_DATA *data, KEY * output);

    /*
     * Functions for Cipher (RC4, DES, 3DES) stuff
     */
/* output : output data buffer */
/* input : input data buffer */
/* key : rc4 key data */
/* index_1 : value of index x from RC4 key structure */
/* index_2 : value of index y from RC4 key structure */
    /*
     * Be carefull : RC4 key should be expanded before calling this method
     * (Should we provide an expand function ??)
     */
    typedef int t_zencod_rc4_cipher(KEY * output, const KEY * input,
                                    const KEY * key, unsigned char *index_1,
                                    unsigned char *index_2, int mode);

/* output : output data buffer */
/* input : input data buffer */
/* key_1 : des first key data */
/* key_2 : des second key data */
/* key_3 : des third key data */
/* iv : initial vector */
/* mode : xdes mode (encrypt or decrypt) */
/* Be carefull : In DES mode key_1 = key_2 = key_3 (as far as i can see !!) */
    typedef int t_zencod_xdes_cipher(KEY * output, const KEY * input,
                                     const KEY * key_1, const KEY * key_2,
                                     const KEY * key_3, const KEY * iv,
                                     int mode);

# undef KEY

# ifdef  __cplusplus
}
# endif                         /* __cplusplus */
#endif                          /* !_HW_ZENCOD_H_ */
