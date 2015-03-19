
#ifndef __ICA_OPENSSL_API_H__
# define __ICA_OPENSSL_API_H__

/**
 ** abstract data types for API
 **/

# define ICA_ADAPTER_HANDLE int

# if defined(linux) || defined (_AIX)
#  define ICA_CALL
# endif

# if defined(WIN32) || defined(_WIN32)
#  define ICA_CALL  __stdcall
# endif

/* -----------------------------------------------*
 | RSA defines and typedefs                       |
 *------------------------------------------------*/
 /*
  * All data elements of the RSA key are in big-endian format
  * Modulus-Exponent form of key
  *
  */
# define MAX_EXP_SIZE 256
# define MAX_MODULUS_SIZE 256
# define MAX_MODEXP_SIZE  (MAX_EXP_SIZE + MAX_MODULUS_SIZE)

# define MAX_OPERAND_SIZE  MAX_EXP_SIZE

typedef unsigned char ICA_KEY_RSA_MODEXPO_REC[MAX_MODEXP_SIZE];
 /*
  * All data elements of the RSA key are in big-endian format
  * Chinese Remainder Thereom(CRT) form of key
  * Used only for Decrypt, the encrypt form is typically Modulus-Exponent
  *
  */
# define MAX_BP_SIZE 136
# define MAX_BQ_SIZE 128
# define MAX_NP_SIZE 136
# define MAX_NQ_SIZE 128
# define MAX_QINV_SIZE 136
# define MAX_RSACRT_SIZE (MAX_BP_SIZE+MAX_BQ_SIZE+MAX_NP_SIZE+MAX_NQ_SIZE+MAX_QINV_SIZE)

# define RSA_GEN_OPERAND_MAX   256/* bytes */

typedef unsigned char ICA_KEY_RSA_CRT_REC[MAX_RSACRT_SIZE];
/* -----------------------------------------------*
 | RSA key token types                            |
 *------------------------------------------------*/

# define  RSA_PUBLIC_MODULUS_EXPONENT        3
# define  RSA_PKCS_PRIVATE_CHINESE_REMAINDER 6

# define KEYTYPE_MODEXPO         1
# define KEYTYPE_PKCSCRT         2

/* -----------------------------------------------*
 | RSA Key Token format                           |
 *------------------------------------------------*/

/*-
 * NOTE:  All the fields in the ICA_KEY_RSA_MODEXPO structure
 *        (lengths, offsets, exponents, modulus, etc.) are
 *        stored in big-endian format
 */

typedef struct _ICA_KEY_RSA_MODEXPO {
    unsigned int keyType;       /* RSA key type.  */
    unsigned int keyLength;     /* Total length of the token.  */
    unsigned int modulusBitLength; /* Modulus n bit length.  */
    /* -- Start of the data length. */
    unsigned int nLength;       /* Modulus n = p * q */
    unsigned int expLength;     /* exponent (public or private) */
    /*   e = 1/d * mod(p-1)(q-1)   */
    /* -- Start of the data offsets */
    unsigned int nOffset;       /* Modulus n .  */
    unsigned int expOffset;     /* exponent (public or private) */
    unsigned char reserved[112]; /* reserved area */
    /* -- Start of the variable -- */
    /* -- length token data.    -- */
    ICA_KEY_RSA_MODEXPO_REC keyRecord;
} ICA_KEY_RSA_MODEXPO;
# define SZ_HEADER_MODEXPO (sizeof(ICA_KEY_RSA_MODEXPO) - sizeof(ICA_KEY_RSA_MODEXPO_REC))

/*-
 * NOTE:  All the fields in the ICA_KEY_RSA_CRT structure
 *        (lengths, offsets, exponents, modulus, etc.) are
 *        stored in big-endian format
 */

typedef struct _ICA_KEY_RSA_CRT {
    unsigned int keyType;       /* RSA key type.  */
    unsigned int keyLength;     /* Total length of the token.  */
    unsigned int modulusBitLength; /* Modulus n bit length.  */
    /* -- Start of the data length. */
# if _AIX
    unsigned int nLength;       /* Modulus n = p * q */
# endif
    unsigned int pLength;       /* Prime number p .  */
    unsigned int qLength;       /* Prime number q .  */
    unsigned int dpLength;      /* dp = d * mod(p-1) .  */
    unsigned int dqLength;      /* dq = d * mod(q-1) .  */
    unsigned int qInvLength;    /* PKCS: qInv = Ap/q */
    /* -- Start of the data offsets */
# if _AIX
    unsigned int nOffset;       /* Modulus n .  */
# endif
    unsigned int pOffset;       /* Prime number p .  */
    unsigned int qOffset;       /* Prime number q .  */
    unsigned int dpOffset;      /* dp .  */
    unsigned int dqOffset;      /* dq .  */
    unsigned int qInvOffset;    /* qInv for PKCS */
# if _AIX
    unsigned char reserved[80]; /* reserved area */
# else
    unsigned char reserved[88]; /* reserved area */
# endif
    /* -- Start of the variable -- */
    /* -- length token data.    -- */
    ICA_KEY_RSA_CRT_REC keyRecord;
} ICA_KEY_RSA_CRT;
# define SZ_HEADER_CRT (sizeof(ICA_KEY_RSA_CRT) - sizeof(ICA_KEY_RSA_CRT_REC))

unsigned int
icaOpenAdapter(unsigned int adapterId, ICA_ADAPTER_HANDLE * pAdapterHandle);

unsigned int icaCloseAdapter(ICA_ADAPTER_HANDLE adapterHandle);

unsigned int
icaRsaModExpo(ICA_ADAPTER_HANDLE hAdapterHandle,
              unsigned int inputDataLength,
              unsigned char *pInputData,
              ICA_KEY_RSA_MODEXPO *pKeyModExpo,
              unsigned int *pOutputDataLength, unsigned char *pOutputData);

unsigned int
icaRsaCrt(ICA_ADAPTER_HANDLE hAdapterHandle,
          unsigned int inputDataLength,
          unsigned char *pInputData,
          ICA_KEY_RSA_CRT *pKeyCrt,
          unsigned int *pOutputDataLength, unsigned char *pOutputData);

unsigned int
icaRandomNumberGenerate(ICA_ADAPTER_HANDLE hAdapterHandle,
                        unsigned int outputDataLength,
                        unsigned char *pOutputData);

/*
 * Specific macros and definitions to not have IFDEF;s all over the main code
 */

# if (_AIX)
static const char *IBMCA_LIBNAME = "/lib/libica.a(shr.o)";
# elif (WIN32)
static const char *IBMCA_LIBNAME = "cryptica";
# else
static const char *IBMCA_LIBNAME = "ica";
# endif

# if (WIN32)
/*
 * The ICA_KEY_RSA_MODEXPO & ICA_KEY_RSA_CRT lengths and offsets must be in
 * big-endian format.
 *
 */
#  define CORRECT_ENDIANNESS(b) (  \
                             (((unsigned long) (b) & 0x000000ff) << 24) |  \
                             (((unsigned long) (b) & 0x0000ff00) <<  8) |  \
                             (((unsigned long) (b) & 0x00ff0000) >>  8) |  \
                             (((unsigned long) (b) & 0xff000000) >> 24)    \
                             )
#  define CRT_KEY_TYPE   RSA_PKCS_PRIVATE_CHINESE_REMAINDER
#  define ME_KEY_TYPE    RSA_PUBLIC_MODULUS_EXPONENT
# else
#  define CORRECT_ENDIANNESS(b) (b)
#  define CRT_KEY_TYPE       KEYTYPE_PKCSCRT
#  define ME_KEY_TYPE        KEYTYPE_MODEXPO
# endif

#endif                          /* __ICA_OPENSSL_API_H__ */
