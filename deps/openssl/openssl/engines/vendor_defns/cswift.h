/*
 * Attribution notice: Rainbow have generously allowed me to reproduce the
 * necessary definitions here from their API. This means the support can
 * build independently of whether application builders have the API or
 * hardware. This will allow developers to easily produce software that has
 * latent hardware support for any users that have accelertors installed,
 * without the developers themselves needing anything extra. I have only
 * clipped the parts from the CryptoSwift header files that are (or seem)
 * relevant to the CryptoSwift support code. This is simply to keep the file
 * sizes reasonable. [Geoff]
 */

/*
 * NB: These type widths do *not* seem right in general, in particular
 * they're not terribly friendly to 64-bit architectures (unsigned long) will
 * be 64-bit on IA-64 for a start. I'm leaving these alone as they agree with
 * Rainbow's API and this will only be called into question on platforms with
 * Rainbow support anyway! ;-)
 */

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

    typedef long SW_STATUS;     /* status */
    typedef unsigned char SW_BYTE; /* 8 bit byte */
    typedef unsigned short SW_U16; /* 16 bit number */
#if defined(_IRIX)
# include <sgidefs.h>
    typedef __uint32_t SW_U32;
#else
    typedef unsigned long SW_U32; /* 32 bit integer */
#endif

#if defined(OPENSSL_SYS_WIN32)
    typedef struct _SW_U64 {
        SW_U32 low32;
        SW_U32 high32;
    } SW_U64;                   /* 64 bit integer */
#elif defined(OPENSSL_SYS_MACINTOSH_CLASSIC)
    typedef longlong SW_U64
#else                           /* Unix variants */
    typedef struct _SW_U64 {
        SW_U32 low32;
        SW_U32 high32;
    } SW_U64;                   /* 64 bit integer */
#endif

/* status codes */
#define SW_OK                 (0L)
#define SW_ERR_BASE           (-10000L)
#define SW_ERR_NO_CARD        (SW_ERR_BASE-1) /* The Card is not present */
#define SW_ERR_CARD_NOT_READY (SW_ERR_BASE-2) /* The card has not powered */
    /*    up yet                 */
#define SW_ERR_TIME_OUT       (SW_ERR_BASE-3) /* Execution of a command */
    /*    time out               */
#define SW_ERR_NO_EXECUTE     (SW_ERR_BASE-4) /* The Card failed to */
    /*    execute the command    */
#define SW_ERR_INPUT_NULL_PTR (SW_ERR_BASE-5) /* a required pointer is */
    /*    NULL                   */
#define SW_ERR_INPUT_SIZE     (SW_ERR_BASE-6) /* size is invalid, too */
    /*    small, too large.      */
#define SW_ERR_INVALID_HANDLE (SW_ERR_BASE-7) /* Invalid SW_ACC_CONTEXT */
    /*    handle                 */
#define SW_ERR_PENDING        (SW_ERR_BASE-8) /* A request is already out- */
    /*    standing at this       */
    /*    context handle         */
#define SW_ERR_AVAILABLE      (SW_ERR_BASE-9) /* A result is available.  */
#define SW_ERR_NO_PENDING     (SW_ERR_BASE-10) /* No request is pending.  */
#define SW_ERR_NO_MEMORY      (SW_ERR_BASE-11) /* Not enough memory */
#define SW_ERR_BAD_ALGORITHM  (SW_ERR_BASE-12) /* Invalid algorithm type */
    /*    in SW_PARAM structure  */
#define SW_ERR_MISSING_KEY    (SW_ERR_BASE-13) /* No key is associated with */
    /*    context.               */
    /*    swAttachKeyParam() is  */
    /*    not called.            */
#define SW_ERR_KEY_CMD_MISMATCH \
                              (SW_ERR_BASE-14) /* Cannot perform requested */
    /*    SW_COMMAND_CODE since  */
    /*    key attached via       */
    /*    swAttachKeyParam()     */
    /*    cannot be used for this */
    /*    SW_COMMAND_CODE.       */
#define SW_ERR_NOT_IMPLEMENTED \
                              (SW_ERR_BASE-15) /* Not implemented */
#define SW_ERR_BAD_COMMAND    (SW_ERR_BASE-16) /* Bad command code */
#define SW_ERR_BAD_ITEM_SIZE  (SW_ERR_BASE-17) /* too small or too large in */
    /*    the "initems" or       */
    /*    "outitems".            */
#define SW_ERR_BAD_ACCNUM     (SW_ERR_BASE-18) /* Bad accelerator number */
#define SW_ERR_SELFTEST_FAIL  (SW_ERR_BASE-19) /* At least one of the self */
    /*    test fail, look at the */
    /*    selfTestBitmap in      */
    /*    SW_ACCELERATOR_INFO for */
    /*    details.               */
#define SW_ERR_MISALIGN       (SW_ERR_BASE-20) /* Certain alogrithms require */
    /*    key materials aligned  */
    /*    in certain order, e.g. */
    /*    128 bit for CRT        */
#define SW_ERR_OUTPUT_NULL_PTR \
                              (SW_ERR_BASE-21) /* a required pointer is */
    /*    NULL                   */
#define SW_ERR_OUTPUT_SIZE \
                              (SW_ERR_BASE-22) /* size is invalid, too */
    /*    small, too large.      */
#define SW_ERR_FIRMWARE_CHECKSUM \
                              (SW_ERR_BASE-23) /* firmware checksum mismatch */
    /*    download failed.       */
#define SW_ERR_UNKNOWN_FIRMWARE \
                              (SW_ERR_BASE-24) /* unknown firmware error */
#define SW_ERR_INTERRUPT      (SW_ERR_BASE-25) /* request is abort when */
    /*    it's waiting to be     */
    /*    completed.             */
#define SW_ERR_NVWRITE_FAIL   (SW_ERR_BASE-26) /* error in writing to Non- */
    /*    volatile memory        */
#define SW_ERR_NVWRITE_RANGE  (SW_ERR_BASE-27) /* out of range error in */
    /*    writing to NV memory   */
#define SW_ERR_RNG_ERROR      (SW_ERR_BASE-28) /* Random Number Generation */
    /*    failure                */
#define SW_ERR_DSS_FAILURE    (SW_ERR_BASE-29) /* DSS Sign or Verify failure */
#define SW_ERR_MODEXP_FAILURE (SW_ERR_BASE-30) /* Failure in various math */
    /*    calculations           */
#define SW_ERR_ONBOARD_MEMORY (SW_ERR_BASE-31) /* Error in accessing on - */
    /*    board memory           */
#define SW_ERR_FIRMWARE_VERSION \
                              (SW_ERR_BASE-32) /* Wrong version in firmware */
    /*    update                 */
#define SW_ERR_ZERO_WORKING_ACCELERATOR \
                              (SW_ERR_BASE-44) /* All accelerators are bad */

    /* algorithm type */
#define SW_ALG_CRT          1
#define SW_ALG_EXP          2
#define SW_ALG_DSA          3
#define SW_ALG_NVDATA       4

    /* command code */
#define SW_CMD_MODEXP_CRT   1   /* perform Modular Exponentiation using */
    /*  Chinese Remainder Theorem (CRT)      */
#define SW_CMD_MODEXP       2   /* perform Modular Exponentiation */
#define SW_CMD_DSS_SIGN     3   /* perform DSS sign */
#define SW_CMD_DSS_VERIFY   4   /* perform DSS verify */
#define SW_CMD_RAND         5   /* perform random number generation */
#define SW_CMD_NVREAD       6   /* perform read to nonvolatile RAM */
#define SW_CMD_NVWRITE      7   /* perform write to nonvolatile RAM */

    typedef SW_U32 SW_ALGTYPE;  /* alogrithm type */
    typedef SW_U32 SW_STATE;    /* state */
    typedef SW_U32 SW_COMMAND_CODE; /* command code */
    typedef SW_U32 SW_COMMAND_BITMAP[4]; /* bitmap */

    typedef struct _SW_LARGENUMBER {
        SW_U32 nbytes;          /* number of bytes in the buffer "value" */
        SW_BYTE *value;         /* the large integer as a string of */
        /*   bytes in network (big endian) order  */
    } SW_LARGENUMBER;

#if defined(OPENSSL_SYS_WIN32)
# include <windows.h>
    typedef HANDLE SW_OSHANDLE; /* handle to kernel object */
# define SW_OS_INVALID_HANDLE  INVALID_HANDLE_VALUE
# define SW_CALLCONV _stdcall
#elif defined(OPENSSL_SYS_MACINTOSH_CLASSIC)
    /* async callback mechanisms */
    /* swiftCallbackLevel */
# define SW_MAC_CALLBACK_LEVEL_NO         0
# define SW_MAC_CALLBACK_LEVEL_HARDWARE   1/* from the hardware ISR */
# define SW_MAC_CALLBACK_LEVEL_SECONDARY  2/* as secondary ISR */
    typedef int SW_MAC_CALLBACK_LEVEL;
    typedef int SW_OSHANDLE;
# define SW_OS_INVALID_HANDLE  (-1)
# define SW_CALLCONV
#else                           /* Unix variants */
    typedef int SW_OSHANDLE;    /* handle to driver */
# define SW_OS_INVALID_HANDLE  (-1)
# define SW_CALLCONV
#endif

    typedef struct _SW_CRT {
        SW_LARGENUMBER p;       /* prime number p */
        SW_LARGENUMBER q;       /* prime number q */
        SW_LARGENUMBER dmp1;    /* exponent1 */
        SW_LARGENUMBER dmq1;    /* exponent2 */
        SW_LARGENUMBER iqmp;    /* CRT coefficient */
    } SW_CRT;

    typedef struct _SW_EXP {
        SW_LARGENUMBER modulus; /* modulus */
        SW_LARGENUMBER exponent; /* exponent */
    } SW_EXP;

    typedef struct _SW_DSA {
        SW_LARGENUMBER p;       /* */
        SW_LARGENUMBER q;       /* */
        SW_LARGENUMBER g;       /* */
        SW_LARGENUMBER key;     /* private/public key */
    } SW_DSA;

    typedef struct _SW_NVDATA {
        SW_U32 accnum;          /* accelerator board number */
        SW_U32 offset;          /* offset in byte */
    } SW_NVDATA;

    typedef struct _SW_PARAM {
        SW_ALGTYPE type;        /* type of the alogrithm */
        union {
            SW_CRT crt;
            SW_EXP exp;
            SW_DSA dsa;
            SW_NVDATA nvdata;
        } up;
    } SW_PARAM;

    typedef SW_U32 SW_CONTEXT_HANDLE; /* opaque context handle */

    /*
     * Now the OpenSSL bits, these function types are the for the function
     * pointers that will bound into the Rainbow shared libraries.
     */
    typedef SW_STATUS SW_CALLCONV t_swAcquireAccContext(SW_CONTEXT_HANDLE
                                                        *hac);
    typedef SW_STATUS SW_CALLCONV t_swAttachKeyParam(SW_CONTEXT_HANDLE hac,
                                                     SW_PARAM *key_params);
    typedef SW_STATUS SW_CALLCONV t_swSimpleRequest(SW_CONTEXT_HANDLE hac,
                                                    SW_COMMAND_CODE cmd,
                                                    SW_LARGENUMBER pin[],
                                                    SW_U32 pin_count,
                                                    SW_LARGENUMBER pout[],
                                                    SW_U32 pout_count);
    typedef SW_STATUS SW_CALLCONV t_swReleaseAccContext(SW_CONTEXT_HANDLE
                                                        hac);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
