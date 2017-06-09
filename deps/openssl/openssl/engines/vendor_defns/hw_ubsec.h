/******************************************************************************
 *
 *  Copyright 2000
 *  Broadcom Corporation
 *  16215 Alton Parkway
 *  PO Box 57013
 *  Irvine CA 92619-7013
 *
 *****************************************************************************/
/*
 * Broadcom Corporation uBSec SDK
 */
/*
 * Character device header file.
 */
/*
 * Revision History:
 *
 * October 2000 JTT Created.
 */

#define MAX_PUBLIC_KEY_BITS (1024)
#define MAX_PUBLIC_KEY_BYTES (1024/8)
#define SHA_BIT_SIZE  (160)
#define MAX_CRYPTO_KEY_LENGTH 24
#define MAX_MAC_KEY_LENGTH 64
#define UBSEC_CRYPTO_DEVICE_NAME ((unsigned char *)"/dev/ubscrypt")
#define UBSEC_KEY_DEVICE_NAME ((unsigned char *)"/dev/ubskey")

/* Math command types. */
#define UBSEC_MATH_MODADD 0x0001
#define UBSEC_MATH_MODSUB 0x0002
#define UBSEC_MATH_MODMUL 0x0004
#define UBSEC_MATH_MODEXP 0x0008
#define UBSEC_MATH_MODREM 0x0010
#define UBSEC_MATH_MODINV 0x0020

typedef long ubsec_MathCommand_t;
typedef long ubsec_RNGCommand_t;

typedef struct ubsec_crypto_context_s {
    unsigned int flags;
    unsigned char crypto[MAX_CRYPTO_KEY_LENGTH];
    unsigned char auth[MAX_MAC_KEY_LENGTH];
} ubsec_crypto_context_t, *ubsec_crypto_context_p;

/*
 * Predeclare the function pointer types that we dynamically load from the DSO.
 */

typedef int t_UBSEC_ubsec_bytes_to_bits(unsigned char *n, int bytes);

typedef int t_UBSEC_ubsec_bits_to_bytes(int bits);

typedef int t_UBSEC_ubsec_open(unsigned char *device);

typedef int t_UBSEC_ubsec_close(int fd);

typedef int t_UBSEC_diffie_hellman_generate_ioctl(int fd,
                                                  unsigned char *x,
                                                  int *x_len,
                                                  unsigned char *y,
                                                  int *y_len,
                                                  unsigned char *g, int g_len,
                                                  unsigned char *m, int m_len,
                                                  unsigned char *userX,
                                                  int userX_len,
                                                  int random_bits);

typedef int t_UBSEC_diffie_hellman_agree_ioctl(int fd,
                                               unsigned char *x, int x_len,
                                               unsigned char *y, int y_len,
                                               unsigned char *m, int m_len,
                                               unsigned char *k, int *k_len);

typedef int t_UBSEC_rsa_mod_exp_ioctl(int fd,
                                      unsigned char *x, int x_len,
                                      unsigned char *m, int m_len,
                                      unsigned char *e, int e_len,
                                      unsigned char *y, int *y_len);

typedef int t_UBSEC_rsa_mod_exp_crt_ioctl(int fd,
                                          unsigned char *x, int x_len,
                                          unsigned char *qinv, int qinv_len,
                                          unsigned char *edq, int edq_len,
                                          unsigned char *q, int q_len,
                                          unsigned char *edp, int edp_len,
                                          unsigned char *p, int p_len,
                                          unsigned char *y, int *y_len);

typedef int t_UBSEC_dsa_sign_ioctl(int fd,
                                   int hash, unsigned char *data,
                                   int data_len, unsigned char *rndom,
                                   int random_len, unsigned char *p,
                                   int p_len, unsigned char *q, int q_len,
                                   unsigned char *g, int g_len,
                                   unsigned char *key, int key_len,
                                   unsigned char *r, int *r_len,
                                   unsigned char *s, int *s_len);

typedef int t_UBSEC_dsa_verify_ioctl(int fd,
                                     int hash, unsigned char *data,
                                     int data_len, unsigned char *p,
                                     int p_len, unsigned char *q, int q_len,
                                     unsigned char *g, int g_len,
                                     unsigned char *key, int key_len,
                                     unsigned char *r, int r_len,
                                     unsigned char *s, int s_len,
                                     unsigned char *v, int *v_len);

typedef int t_UBSEC_math_accelerate_ioctl(int fd, ubsec_MathCommand_t command,
                                          unsigned char *ModN, int *ModN_len,
                                          unsigned char *ExpE, int *ExpE_len,
                                          unsigned char *ParamA,
                                          int *ParamA_len,
                                          unsigned char *ParamB,
                                          int *ParamB_len,
                                          unsigned char *Result,
                                          int *Result_len);

typedef int t_UBSEC_rng_ioctl(int fd, ubsec_RNGCommand_t command,
                              unsigned char *Result, int *Result_len);

typedef int t_UBSEC_max_key_len_ioctl(int fd, int *max_key_len);
