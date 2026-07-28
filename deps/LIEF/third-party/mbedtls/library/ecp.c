/*
 *  Elliptic curves over GF(p): generic functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * References:
 *
 * SEC1 https://www.secg.org/sec1-v2.pdf
 * GECC = Guide to Elliptic Curve Cryptography - Hankerson, Menezes, Vanstone
 * FIPS 186-3 http://csrc.nist.gov/publications/fips/fips186-3/fips_186-3.pdf
 * RFC 4492 for the related TLS structures and constants
 * - https://www.rfc-editor.org/rfc/rfc4492
 * RFC 7748 for the Curve448 and Curve25519 curve definitions
 * - https://www.rfc-editor.org/rfc/rfc7748
 *
 * [Curve25519] https://cr.yp.to/ecdh/curve25519-20060209.pdf
 *
 * [2] CORON, Jean-S'ebastien. Resistance against differential power analysis
 *     for elliptic curve cryptosystems. In : Cryptographic Hardware and
 *     Embedded Systems. Springer Berlin Heidelberg, 1999. p. 292-302.
 *     <http://link.springer.com/chapter/10.1007/3-540-48059-5_25>
 *
 * [3] HEDABOU, Mustapha, PINEL, Pierre, et B'EN'ETEAU, Lucien. A comb method to
 *     render ECC resistant against Side Channel Attacks. IACR Cryptology
 *     ePrint Archive, 2004, vol. 2004, p. 342.
 *     <http://eprint.iacr.org/2004/342.pdf>
 */

#include "common.h"

/**
 * \brief Function level alternative implementation.
 *
 * The MBEDTLS_ECP_INTERNAL_ALT macro enables alternative implementations to
 * replace certain functions in this module. The alternative implementations are
 * typically hardware accelerators and need to activate the hardware before the
 * computation starts and deactivate it after it finishes. The
 * mbedtls_internal_ecp_init() and mbedtls_internal_ecp_free() functions serve
 * this purpose.
 *
 * To preserve the correct functionality the following conditions must hold:
 *
 * - The alternative implementation must be activated by
 *   mbedtls_internal_ecp_init() before any of the replaceable functions is
 *   called.
 * - mbedtls_internal_ecp_free() must \b only be called when the alternative
 *   implementation is activated.
 * - mbedtls_internal_ecp_init() must \b not be called when the alternative
 *   implementation is activated.
 * - Public functions must not return while the alternative implementation is
 *   activated.
 * - Replaceable functions are guarded by \c MBEDTLS_ECP_XXX_ALT macros and
 *   before calling them an \code if( mbedtls_internal_ecp_grp_capable( grp ) )
 *   \endcode ensures that the alternative implementation supports the current
 *   group.
 */
#if defined(MBEDTLS_ECP_INTERNAL_ALT)
#endif

#if defined(MBEDTLS_ECP_LIGHT)

#include "mbedtls/ecp.h"
#include "mbedtls/threading.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#include "bn_mul.h"
#include "ecp_invasive.h"

#include <string.h>

#if !defined(MBEDTLS_ECP_ALT)

#include "mbedtls/platform.h"

#include "ecp_internal_alt.h"

#if defined(MBEDTLS_SELF_TEST)
/*
 * Counts of point addition and doubling, and field multiplications.
 * Used to test resistance of point multiplication to simple timing attacks.
 */
#if defined(MBEDTLS_ECP_C)
static unsigned long add_count, dbl_count;
#endif /* MBEDTLS_ECP_C */
static unsigned long mul_count;
#endif

#if defined(MBEDTLS_ECP_RESTARTABLE)
/*
 * Maximum number of "basic operations" to be done in a row.
 *
 * Default value 0 means that ECC operations will not yield.
 * Note that regardless of the value of ecp_max_ops, always at
 * least one step is performed before yielding.
 *
 * Setting ecp_max_ops=1 can be suitable for testing purposes
 * as it will interrupt computation at all possible points.
 */
static unsigned ecp_max_ops = 0;

/*
 * Set ecp_max_ops
 */
void mbedtls_ecp_set_max_ops(unsigned max_ops)
{
    ecp_max_ops = max_ops;
}

/*
 * Check if restart is enabled
 */
int mbedtls_ecp_restart_is_enabled(void)
{
    return ecp_max_ops != 0;
}

/*
 * Restart sub-context for ecp_mul_comb()
 */
struct mbedtls_ecp_restart_mul {
    mbedtls_ecp_point R;    /* current intermediate result                  */
    size_t i;               /* current index in various loops, 0 outside    */
    mbedtls_ecp_point *T;   /* table for precomputed points                 */
    unsigned char T_size;   /* number of points in table T                  */
    enum {                  /* what were we doing last time we returned?    */
        ecp_rsm_init = 0,       /* nothing so far, dummy initial state      */
        ecp_rsm_pre_dbl,        /* precompute 2^n multiples                 */
        ecp_rsm_pre_norm_dbl,   /* normalize precomputed 2^n multiples      */
        ecp_rsm_pre_add,        /* precompute remaining points by adding    */
        ecp_rsm_pre_norm_add,   /* normalize all precomputed points         */
        ecp_rsm_comb_core,      /* ecp_mul_comb_core()                      */
        ecp_rsm_final_norm,     /* do the final normalization               */
    } state;
};

/*
 * Init restart_mul sub-context
 */
static void ecp_restart_rsm_init(mbedtls_ecp_restart_mul_ctx *ctx)
{
    mbedtls_ecp_point_init(&ctx->R);
    ctx->i = 0;
    ctx->T = NULL;
    ctx->T_size = 0;
    ctx->state = ecp_rsm_init;
}

/*
 * Free the components of a restart_mul sub-context
 */
static void ecp_restart_rsm_free(mbedtls_ecp_restart_mul_ctx *ctx)
{
    unsigned char i;

    if (ctx == NULL) {
        return;
    }

    mbedtls_ecp_point_free(&ctx->R);

    if (ctx->T != NULL) {
        for (i = 0; i < ctx->T_size; i++) {
            mbedtls_ecp_point_free(ctx->T + i);
        }
        mbedtls_free(ctx->T);
    }

    ecp_restart_rsm_init(ctx);
}

/*
 * Restart context for ecp_muladd()
 */
struct mbedtls_ecp_restart_muladd {
    mbedtls_ecp_point mP;       /* mP value                             */
    mbedtls_ecp_point R;        /* R intermediate result                */
    enum {                      /* what should we do next?              */
        ecp_rsma_mul1 = 0,      /* first multiplication                 */
        ecp_rsma_mul2,          /* second multiplication                */
        ecp_rsma_add,           /* addition                             */
        ecp_rsma_norm,          /* normalization                        */
    } state;
};

/*
 * Init restart_muladd sub-context
 */
static void ecp_restart_ma_init(mbedtls_ecp_restart_muladd_ctx *ctx)
{
    mbedtls_ecp_point_init(&ctx->mP);
    mbedtls_ecp_point_init(&ctx->R);
    ctx->state = ecp_rsma_mul1;
}

/*
 * Free the components of a restart_muladd sub-context
 */
static void ecp_restart_ma_free(mbedtls_ecp_restart_muladd_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_ecp_point_free(&ctx->mP);
    mbedtls_ecp_point_free(&ctx->R);

    ecp_restart_ma_init(ctx);
}

/*
 * Initialize a restart context
 */
void mbedtls_ecp_restart_init(mbedtls_ecp_restart_ctx *ctx)
{
    ctx->ops_done = 0;
    ctx->depth = 0;
    ctx->rsm = NULL;
    ctx->ma = NULL;
}

/*
 * Free the components of a restart context
 */
void mbedtls_ecp_restart_free(mbedtls_ecp_restart_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ecp_restart_rsm_free(ctx->rsm);
    mbedtls_free(ctx->rsm);

    ecp_restart_ma_free(ctx->ma);
    mbedtls_free(ctx->ma);

    mbedtls_ecp_restart_init(ctx);
}

/*
 * Check if we can do the next step
 */
int mbedtls_ecp_check_budget(const mbedtls_ecp_group *grp,
                             mbedtls_ecp_restart_ctx *rs_ctx,
                             unsigned ops)
{
    if (rs_ctx != NULL && ecp_max_ops != 0) {
        /* scale depending on curve size: the chosen reference is 256-bit,
         * and multiplication is quadratic. Round to the closest integer. */
        if (grp->pbits >= 512) {
            ops *= 4;
        } else if (grp->pbits >= 384) {
            ops *= 2;
        }

        /* Avoid infinite loops: always allow first step.
         * Because of that, however, it's not generally true
         * that ops_done <= ecp_max_ops, so the check
         * ops_done > ecp_max_ops below is mandatory. */
        if ((rs_ctx->ops_done != 0) &&
            (rs_ctx->ops_done > ecp_max_ops ||
             ops > ecp_max_ops - rs_ctx->ops_done)) {
            return MBEDTLS_ERR_ECP_IN_PROGRESS;
        }

        /* update running count */
        rs_ctx->ops_done += ops;
    }

    return 0;
}

/* Call this when entering a function that needs its own sub-context */
#define ECP_RS_ENTER(SUB)   do {                                      \
        /* reset ops count for this call if top-level */                    \
        if (rs_ctx != NULL && rs_ctx->depth++ == 0)                        \
        rs_ctx->ops_done = 0;                                           \
                                                                        \
        /* set up our own sub-context if needed */                          \
        if (mbedtls_ecp_restart_is_enabled() &&                             \
            rs_ctx != NULL && rs_ctx->SUB == NULL)                         \
        {                                                                   \
            rs_ctx->SUB = mbedtls_calloc(1, sizeof(*rs_ctx->SUB));      \
            if (rs_ctx->SUB == NULL)                                       \
            return MBEDTLS_ERR_ECP_ALLOC_FAILED;                     \
                                                                      \
            ecp_restart_## SUB ##_init(rs_ctx->SUB);                      \
        }                                                                   \
} while (0)

/* Call this when leaving a function that needs its own sub-context */
#define ECP_RS_LEAVE(SUB)   do {                                      \
        /* clear our sub-context when not in progress (done or error) */    \
        if (rs_ctx != NULL && rs_ctx->SUB != NULL &&                        \
            ret != MBEDTLS_ERR_ECP_IN_PROGRESS)                            \
        {                                                                   \
            ecp_restart_## SUB ##_free(rs_ctx->SUB);                      \
            mbedtls_free(rs_ctx->SUB);                                    \
            rs_ctx->SUB = NULL;                                             \
        }                                                                   \
                                                                        \
        if (rs_ctx != NULL)                                                \
        rs_ctx->depth--;                                                \
} while (0)

#else /* MBEDTLS_ECP_RESTARTABLE */

#define ECP_RS_ENTER(sub)     (void) rs_ctx;
#define ECP_RS_LEAVE(sub)     (void) rs_ctx;

#endif /* MBEDTLS_ECP_RESTARTABLE */

#if defined(MBEDTLS_ECP_C)
static void mpi_init_many(mbedtls_mpi *arr, size_t size)
{
    while (size--) {
        mbedtls_mpi_init(arr++);
    }
}

static void mpi_free_many(mbedtls_mpi *arr, size_t size)
{
    while (size--) {
        mbedtls_mpi_free(arr++);
    }
}
#endif /* MBEDTLS_ECP_C */

/*
 * List of supported curves:
 *  - internal ID
 *  - TLS NamedCurve ID (RFC 4492 sec. 5.1.1, RFC 7071 sec. 2, RFC 8446 sec. 4.2.7)
 *  - size in bits
 *  - readable name
 *
 * Curves are listed in order: largest curves first, and for a given size,
 * fastest curves first.
 *
 * Reminder: update profiles in x509_crt.c and ssl_tls.c when adding a new curve!
 */
static const mbedtls_ecp_curve_info ecp_supported_curves[] =
{
#if defined(MBEDTLS_ECP_DP_SECP521R1_ENABLED)
    { MBEDTLS_ECP_DP_SECP521R1,    25,     521,    "secp521r1"         },
#endif
#if defined(MBEDTLS_ECP_DP_BP512R1_ENABLED)
    { MBEDTLS_ECP_DP_BP512R1,      28,     512,    "brainpoolP512r1"   },
#endif
#if defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED)
    { MBEDTLS_ECP_DP_SECP384R1,    24,     384,    "secp384r1"         },
#endif
#if defined(MBEDTLS_ECP_DP_BP384R1_ENABLED)
    { MBEDTLS_ECP_DP_BP384R1,      27,     384,    "brainpoolP384r1"   },
#endif
#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
    { MBEDTLS_ECP_DP_SECP256R1,    23,     256,    "secp256r1"         },
#endif
#if defined(MBEDTLS_ECP_DP_SECP256K1_ENABLED)
    { MBEDTLS_ECP_DP_SECP256K1,    22,     256,    "secp256k1"         },
#endif
#if defined(MBEDTLS_ECP_DP_BP256R1_ENABLED)
    { MBEDTLS_ECP_DP_BP256R1,      26,     256,    "brainpoolP256r1"   },
#endif
#if defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED)
    { MBEDTLS_ECP_DP_SECP224R1,    21,     224,    "secp224r1"         },
#endif
#if defined(MBEDTLS_ECP_DP_SECP224K1_ENABLED)
    { MBEDTLS_ECP_DP_SECP224K1,    20,     224,    "secp224k1"         },
#endif
#if defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED)
    { MBEDTLS_ECP_DP_SECP192R1,    19,     192,    "secp192r1"         },
#endif
#if defined(MBEDTLS_ECP_DP_SECP192K1_ENABLED)
    { MBEDTLS_ECP_DP_SECP192K1,    18,     192,    "secp192k1"         },
#endif
#if defined(MBEDTLS_ECP_DP_CURVE25519_ENABLED)
    { MBEDTLS_ECP_DP_CURVE25519,   29,     256,    "x25519"            },
#endif
#if defined(MBEDTLS_ECP_DP_CURVE448_ENABLED)
    { MBEDTLS_ECP_DP_CURVE448,     30,     448,    "x448"              },
#endif
    { MBEDTLS_ECP_DP_NONE,          0,     0,      NULL                },
};

#define ECP_NB_CURVES   sizeof(ecp_supported_curves) /    \
    sizeof(ecp_supported_curves[0])

static mbedtls_ecp_group_id ecp_supported_grp_id[ECP_NB_CURVES];

/*
 * List of supported curves and associated info
 */
const mbedtls_ecp_curve_info *mbedtls_ecp_curve_list(void)
{
    return ecp_supported_curves;
}

/*
 * List of supported curves, group ID only
 */
const mbedtls_ecp_group_id *mbedtls_ecp_grp_id_list(void)
{
    static int init_done = 0;

    if (!init_done) {
        size_t i = 0;
        const mbedtls_ecp_curve_info *curve_info;

        for (curve_info = mbedtls_ecp_curve_list();
             curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
             curve_info++) {
            ecp_supported_grp_id[i++] = curve_info->grp_id;
        }
        ecp_supported_grp_id[i] = MBEDTLS_ECP_DP_NONE;

        init_done = 1;
    }

    return ecp_supported_grp_id;
}

/*
 * Get the curve info for the internal identifier
 */
const mbedtls_ecp_curve_info *mbedtls_ecp_curve_info_from_grp_id(mbedtls_ecp_group_id grp_id)
{
    const mbedtls_ecp_curve_info *curve_info;

    for (curve_info = mbedtls_ecp_curve_list();
         curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
         curve_info++) {
        if (curve_info->grp_id == grp_id) {
            return curve_info;
        }
    }

    return NULL;
}

/*
 * Get the curve info from the TLS identifier
 */
const mbedtls_ecp_curve_info *mbedtls_ecp_curve_info_from_tls_id(uint16_t tls_id)
{
    const mbedtls_ecp_curve_info *curve_info;

    for (curve_info = mbedtls_ecp_curve_list();
         curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
         curve_info++) {
        if (curve_info->tls_id == tls_id) {
            return curve_info;
        }
    }

    return NULL;
}

/*
 * Get the curve info from the name
 */
const mbedtls_ecp_curve_info *mbedtls_ecp_curve_info_from_name(const char *name)
{
    const mbedtls_ecp_curve_info *curve_info;

    if (name == NULL) {
        return NULL;
    }

    for (curve_info = mbedtls_ecp_curve_list();
         curve_info->grp_id != MBEDTLS_ECP_DP_NONE;
         curve_info++) {
        if (strcmp(curve_info->name, name) == 0) {
            return curve_info;
        }
    }

    return NULL;
}

/*
 * Get the type of a curve
 */
mbedtls_ecp_curve_type mbedtls_ecp_get_type(const mbedtls_ecp_group *grp)
{
    if (grp->G.X.p == NULL) {
        return MBEDTLS_ECP_TYPE_NONE;
    }

    if (grp->G.Y.p == NULL) {
        return MBEDTLS_ECP_TYPE_MONTGOMERY;
    } else {
        return MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS;
    }
}

/*
 * Initialize (the components of) a point
 */
void mbedtls_ecp_point_init(mbedtls_ecp_point *pt)
{
    mbedtls_mpi_init(&pt->X);
    mbedtls_mpi_init(&pt->Y);
    mbedtls_mpi_init(&pt->Z);
}

/*
 * Initialize (the components of) a group
 */
void mbedtls_ecp_group_init(mbedtls_ecp_group *grp)
{
    grp->id = MBEDTLS_ECP_DP_NONE;
    mbedtls_mpi_init(&grp->P);
    mbedtls_mpi_init(&grp->A);
    mbedtls_mpi_init(&grp->B);
    mbedtls_ecp_point_init(&grp->G);
    mbedtls_mpi_init(&grp->N);
    grp->pbits = 0;
    grp->nbits = 0;
    grp->h = 0;
    grp->modp = NULL;
    grp->t_pre = NULL;
    grp->t_post = NULL;
    grp->t_data = NULL;
    grp->T = NULL;
    grp->T_size = 0;
}

/*
 * Initialize (the components of) a key pair
 */
void mbedtls_ecp_keypair_init(mbedtls_ecp_keypair *key)
{
    mbedtls_ecp_group_init(&key->grp);
    mbedtls_mpi_init(&key->d);
    mbedtls_ecp_point_init(&key->Q);
}

/*
 * Unallocate (the components of) a point
 */
void mbedtls_ecp_point_free(mbedtls_ecp_point *pt)
{
    if (pt == NULL) {
        return;
    }

    mbedtls_mpi_free(&(pt->X));
    mbedtls_mpi_free(&(pt->Y));
    mbedtls_mpi_free(&(pt->Z));
}

/*
 * Check that the comb table (grp->T) is static initialized.
 */
static int ecp_group_is_static_comb_table(const mbedtls_ecp_group *grp)
{
#if MBEDTLS_ECP_FIXED_POINT_OPTIM == 1
    return grp->T != NULL && grp->T_size == 0;
#else
    (void) grp;
    return 0;
#endif
}

/*
 * Unallocate (the components of) a group
 */
void mbedtls_ecp_group_free(mbedtls_ecp_group *grp)
{
    size_t i;

    if (grp == NULL) {
        return;
    }

    if (grp->h != 1) {
        mbedtls_mpi_free(&grp->A);
        mbedtls_mpi_free(&grp->B);
        mbedtls_ecp_point_free(&grp->G);

#if !defined(MBEDTLS_ECP_WITH_MPI_UINT)
        mbedtls_mpi_free(&grp->N);
        mbedtls_mpi_free(&grp->P);
#endif
    }

    if (!ecp_group_is_static_comb_table(grp) && grp->T != NULL) {
        for (i = 0; i < grp->T_size; i++) {
            mbedtls_ecp_point_free(&grp->T[i]);
        }
        mbedtls_free(grp->T);
    }

    mbedtls_platform_zeroize(grp, sizeof(mbedtls_ecp_group));
}

/*
 * Unallocate (the components of) a key pair
 */
void mbedtls_ecp_keypair_free(mbedtls_ecp_keypair *key)
{
    if (key == NULL) {
        return;
    }

    mbedtls_ecp_group_free(&key->grp);
    mbedtls_mpi_free(&key->d);
    mbedtls_ecp_point_free(&key->Q);
}

/*
 * Copy the contents of a point
 */
int mbedtls_ecp_copy(mbedtls_ecp_point *P, const mbedtls_ecp_point *Q)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&P->X, &Q->X));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&P->Y, &Q->Y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&P->Z, &Q->Z));

cleanup:
    return ret;
}

/*
 * Copy the contents of a group object
 */
int mbedtls_ecp_group_copy(mbedtls_ecp_group *dst, const mbedtls_ecp_group *src)
{
    return mbedtls_ecp_group_load(dst, src->id);
}

/*
 * Set point to zero
 */
int mbedtls_ecp_set_zero(mbedtls_ecp_point *pt)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&pt->X, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&pt->Y, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&pt->Z, 0));

cleanup:
    return ret;
}

/*
 * Tell if a point is zero
 */
int mbedtls_ecp_is_zero(mbedtls_ecp_point *pt)
{
    return mbedtls_mpi_cmp_int(&pt->Z, 0) == 0;
}

/*
 * Compare two points lazily
 */
int mbedtls_ecp_point_cmp(const mbedtls_ecp_point *P,
                          const mbedtls_ecp_point *Q)
{
    if (mbedtls_mpi_cmp_mpi(&P->X, &Q->X) == 0 &&
        mbedtls_mpi_cmp_mpi(&P->Y, &Q->Y) == 0 &&
        mbedtls_mpi_cmp_mpi(&P->Z, &Q->Z) == 0) {
        return 0;
    }

    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
}

/*
 * Import a non-zero point from ASCII strings
 */
int mbedtls_ecp_point_read_string(mbedtls_ecp_point *P, int radix,
                                  const char *x, const char *y)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&P->X, radix, x));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(&P->Y, radix, y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&P->Z, 1));

cleanup:
    return ret;
}

/*
 * Export a point into unsigned binary data (SEC1 2.3.3 and RFC7748)
 */
int mbedtls_ecp_point_write_binary(const mbedtls_ecp_group *grp,
                                   const mbedtls_ecp_point *P,
                                   int format, size_t *olen,
                                   unsigned char *buf, size_t buflen)
{
    int ret = MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    size_t plen;
    if (format != MBEDTLS_ECP_PF_UNCOMPRESSED &&
        format != MBEDTLS_ECP_PF_COMPRESSED) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    plen = mbedtls_mpi_size(&grp->P);

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
    (void) format; /* Montgomery curves always use the same point format */
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_MONTGOMERY) {
        *olen = plen;
        if (buflen < *olen) {
            return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
        }

        MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&P->X, buf, plen));
    }
#endif
#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS) {
        /*
         * Common case: P == 0
         */
        if (mbedtls_mpi_cmp_int(&P->Z, 0) == 0) {
            if (buflen < 1) {
                return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
            }

            buf[0] = 0x00;
            *olen = 1;

            return 0;
        }

        if (format == MBEDTLS_ECP_PF_UNCOMPRESSED) {
            *olen = 2 * plen + 1;

            if (buflen < *olen) {
                return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
            }

            buf[0] = 0x04;
            MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&P->X, buf + 1, plen));
            MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&P->Y, buf + 1 + plen, plen));
        } else if (format == MBEDTLS_ECP_PF_COMPRESSED) {
            *olen = plen + 1;

            if (buflen < *olen) {
                return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
            }

            buf[0] = 0x02 + mbedtls_mpi_get_bit(&P->Y, 0);
            MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&P->X, buf + 1, plen));
        }
    }
#endif

cleanup:
    return ret;
}

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
static int mbedtls_ecp_sw_derive_y(const mbedtls_ecp_group *grp,
                                   const mbedtls_mpi *X,
                                   mbedtls_mpi *Y,
                                   int parity_bit);
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

/*
 * Import a point from unsigned binary data (SEC1 2.3.4 and RFC7748)
 */
int mbedtls_ecp_point_read_binary(const mbedtls_ecp_group *grp,
                                  mbedtls_ecp_point *pt,
                                  const unsigned char *buf, size_t ilen)
{
    int ret = MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    size_t plen;
    if (ilen < 1) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    plen = mbedtls_mpi_size(&grp->P);

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_MONTGOMERY) {
        if (plen != ilen) {
            return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        }

        MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary_le(&pt->X, buf, plen));
        mbedtls_mpi_free(&pt->Y);

        if (grp->id == MBEDTLS_ECP_DP_CURVE25519) {
            /* Set most significant bit to 0 as prescribed in RFC7748 §5 */
            MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(&pt->X, plen * 8 - 1, 0));
        }

        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&pt->Z, 1));
    }
#endif
#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS) {
        if (buf[0] == 0x00) {
            if (ilen == 1) {
                return mbedtls_ecp_set_zero(pt);
            } else {
                return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
            }
        }

        if (ilen < 1 + plen) {
            return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        }

        MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&pt->X, buf + 1, plen));
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&pt->Z, 1));

        if (buf[0] == 0x04) {
            /* format == MBEDTLS_ECP_PF_UNCOMPRESSED */
            if (ilen != 1 + plen * 2) {
                return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
            }
            return mbedtls_mpi_read_binary(&pt->Y, buf + 1 + plen, plen);
        } else if (buf[0] == 0x02 || buf[0] == 0x03) {
            /* format == MBEDTLS_ECP_PF_COMPRESSED */
            if (ilen != 1 + plen) {
                return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
            }
            return mbedtls_ecp_sw_derive_y(grp, &pt->X, &pt->Y,
                                           (buf[0] & 1));
        } else {
            return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        }
    }
#endif

cleanup:
    return ret;
}

/*
 * Import a point from a TLS ECPoint record (RFC 4492)
 *      struct {
 *          opaque point <1..2^8-1>;
 *      } ECPoint;
 */
int mbedtls_ecp_tls_read_point(const mbedtls_ecp_group *grp,
                               mbedtls_ecp_point *pt,
                               const unsigned char **buf, size_t buf_len)
{
    unsigned char data_len;
    const unsigned char *buf_start;
    /*
     * We must have at least two bytes (1 for length, at least one for data)
     */
    if (buf_len < 2) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    data_len = *(*buf)++;
    if (data_len < 1 || data_len > buf_len - 1) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /*
     * Save buffer start for read_binary and update buf
     */
    buf_start = *buf;
    *buf += data_len;

    return mbedtls_ecp_point_read_binary(grp, pt, buf_start, data_len);
}

/*
 * Export a point as a TLS ECPoint record (RFC 4492)
 *      struct {
 *          opaque point <1..2^8-1>;
 *      } ECPoint;
 */
int mbedtls_ecp_tls_write_point(const mbedtls_ecp_group *grp, const mbedtls_ecp_point *pt,
                                int format, size_t *olen,
                                unsigned char *buf, size_t blen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    if (format != MBEDTLS_ECP_PF_UNCOMPRESSED &&
        format != MBEDTLS_ECP_PF_COMPRESSED) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /*
     * buffer length must be at least one, for our length byte
     */
    if (blen < 1) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    if ((ret = mbedtls_ecp_point_write_binary(grp, pt, format,
                                              olen, buf + 1, blen - 1)) != 0) {
        return ret;
    }

    /*
     * write length to the first byte and update total length
     */
    buf[0] = (unsigned char) *olen;
    ++*olen;

    return 0;
}

/*
 * Set a group from an ECParameters record (RFC 4492)
 */
int mbedtls_ecp_tls_read_group(mbedtls_ecp_group *grp,
                               const unsigned char **buf, size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_group_id grp_id;
    if ((ret = mbedtls_ecp_tls_read_group_id(&grp_id, buf, len)) != 0) {
        return ret;
    }

    return mbedtls_ecp_group_load(grp, grp_id);
}

/*
 * Read a group id from an ECParameters record (RFC 4492) and convert it to
 * mbedtls_ecp_group_id.
 */
int mbedtls_ecp_tls_read_group_id(mbedtls_ecp_group_id *grp,
                                  const unsigned char **buf, size_t len)
{
    uint16_t tls_id;
    const mbedtls_ecp_curve_info *curve_info;
    /*
     * We expect at least three bytes (see below)
     */
    if (len < 3) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /*
     * First byte is curve_type; only named_curve is handled
     */
    if (*(*buf)++ != MBEDTLS_ECP_TLS_NAMED_CURVE) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /*
     * Next two bytes are the namedcurve value
     */
    tls_id = MBEDTLS_GET_UINT16_BE(*buf, 0);
    *buf += 2;

    if ((curve_info = mbedtls_ecp_curve_info_from_tls_id(tls_id)) == NULL) {
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }

    *grp = curve_info->grp_id;

    return 0;
}

/*
 * Write the ECParameters record corresponding to a group (RFC 4492)
 */
int mbedtls_ecp_tls_write_group(const mbedtls_ecp_group *grp, size_t *olen,
                                unsigned char *buf, size_t blen)
{
    const mbedtls_ecp_curve_info *curve_info;
    if ((curve_info = mbedtls_ecp_curve_info_from_grp_id(grp->id)) == NULL) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /*
     * We are going to write 3 bytes (see below)
     */
    *olen = 3;
    if (blen < *olen) {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    /*
     * First byte is curve_type, always named_curve
     */
    *buf++ = MBEDTLS_ECP_TLS_NAMED_CURVE;

    /*
     * Next two bytes are the namedcurve value
     */
    MBEDTLS_PUT_UINT16_BE(curve_info->tls_id, buf, 0);

    return 0;
}

/*
 * Wrapper around fast quasi-modp functions, with fall-back to mbedtls_mpi_mod_mpi.
 * See the documentation of struct mbedtls_ecp_group.
 *
 * This function is in the critial loop for mbedtls_ecp_mul, so pay attention to perf.
 */
static int ecp_modp(mbedtls_mpi *N, const mbedtls_ecp_group *grp)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (grp->modp == NULL) {
        return mbedtls_mpi_mod_mpi(N, N, &grp->P);
    }

    /* N->s < 0 is a much faster test, which fails only if N is 0 */
    if ((N->s < 0 && mbedtls_mpi_cmp_int(N, 0) != 0) ||
        mbedtls_mpi_bitlen(N) > 2 * grp->pbits) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    MBEDTLS_MPI_CHK(grp->modp(N));

    /* N->s < 0 is a much faster test, which fails only if N is 0 */
    while (N->s < 0 && mbedtls_mpi_cmp_int(N, 0) != 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(N, N, &grp->P));
    }

    while (mbedtls_mpi_cmp_mpi(N, &grp->P) >= 0) {
        /* we known P, N and the result are positive */
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs(N, N, &grp->P));
    }

cleanup:
    return ret;
}

/*
 * Fast mod-p functions expect their argument to be in the 0..p^2 range.
 *
 * In order to guarantee that, we need to ensure that operands of
 * mbedtls_mpi_mul_mpi are in the 0..p range. So, after each operation we will
 * bring the result back to this range.
 *
 * The following macros are shortcuts for doing that.
 */

/*
 * Reduce a mbedtls_mpi mod p in-place, general case, to use after mbedtls_mpi_mul_mpi
 */
#if defined(MBEDTLS_SELF_TEST)
#define INC_MUL_COUNT   mul_count++;
#else
#define INC_MUL_COUNT
#endif

#define MOD_MUL(N)                                                    \
    do                                                                  \
    {                                                                   \
        MBEDTLS_MPI_CHK(ecp_modp(&(N), grp));                       \
        INC_MUL_COUNT                                                   \
    } while (0)

static inline int mbedtls_mpi_mul_mod(const mbedtls_ecp_group *grp,
                                      mbedtls_mpi *X,
                                      const mbedtls_mpi *A,
                                      const mbedtls_mpi *B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(X, A, B));
    MOD_MUL(*X);
cleanup:
    return ret;
}

/*
 * Reduce a mbedtls_mpi mod p in-place, to use after mbedtls_mpi_sub_mpi
 * N->s < 0 is a very fast test, which fails only if N is 0
 */
#define MOD_SUB(N)                                                          \
    do {                                                                      \
        while ((N)->s < 0 && mbedtls_mpi_cmp_int((N), 0) != 0)             \
        MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi((N), (N), &grp->P));      \
    } while (0)

MBEDTLS_MAYBE_UNUSED
static inline int mbedtls_mpi_sub_mod(const mbedtls_ecp_group *grp,
                                      mbedtls_mpi *X,
                                      const mbedtls_mpi *A,
                                      const mbedtls_mpi *B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(X, A, B));
    MOD_SUB(X);
cleanup:
    return ret;
}

/*
 * Reduce a mbedtls_mpi mod p in-place, to use after mbedtls_mpi_add_mpi and mbedtls_mpi_mul_int.
 * We known P, N and the result are positive, so sub_abs is correct, and
 * a bit faster.
 */
#define MOD_ADD(N)                                                   \
    while (mbedtls_mpi_cmp_mpi((N), &grp->P) >= 0)                  \
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_abs((N), (N), &grp->P))

static inline int mbedtls_mpi_add_mod(const mbedtls_ecp_group *grp,
                                      mbedtls_mpi *X,
                                      const mbedtls_mpi *A,
                                      const mbedtls_mpi *B)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(X, A, B));
    MOD_ADD(X);
cleanup:
    return ret;
}

MBEDTLS_MAYBE_UNUSED
static inline int mbedtls_mpi_mul_int_mod(const mbedtls_ecp_group *grp,
                                          mbedtls_mpi *X,
                                          const mbedtls_mpi *A,
                                          mbedtls_mpi_uint c)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_int(X, A, c));
    MOD_ADD(X);
cleanup:
    return ret;
}

MBEDTLS_MAYBE_UNUSED
static inline int mbedtls_mpi_sub_int_mod(const mbedtls_ecp_group *grp,
                                          mbedtls_mpi *X,
                                          const mbedtls_mpi *A,
                                          mbedtls_mpi_uint c)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(X, A, c));
    MOD_SUB(X);
cleanup:
    return ret;
}

#define MPI_ECP_SUB_INT(X, A, c)             \
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int_mod(grp, X, A, c))

MBEDTLS_MAYBE_UNUSED
static inline int mbedtls_mpi_shift_l_mod(const mbedtls_ecp_group *grp,
                                          mbedtls_mpi *X,
                                          size_t count)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l(X, count));
    MOD_ADD(X);
cleanup:
    return ret;
}

/*
 * Macro wrappers around ECP modular arithmetic
 *
 * Currently, these wrappers are defined via the bignum module.
 */

#define MPI_ECP_ADD(X, A, B)                                                  \
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mod(grp, X, A, B))

#define MPI_ECP_SUB(X, A, B)                                                  \
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mod(grp, X, A, B))

#define MPI_ECP_MUL(X, A, B)                                                  \
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, X, A, B))

#define MPI_ECP_SQR(X, A)                                                     \
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mod(grp, X, A, A))

#define MPI_ECP_MUL_INT(X, A, c)                                              \
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_int_mod(grp, X, A, c))

#define MPI_ECP_INV(dst, src)                                                 \
    MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod((dst), (src), &grp->P))

#define MPI_ECP_MOV(X, A)                                                     \
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(X, A))

#define MPI_ECP_SHIFT_L(X, count)                                             \
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_l_mod(grp, X, count))

#define MPI_ECP_LSET(X, c)                                                    \
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(X, c))

#define MPI_ECP_CMP_INT(X, c)                                                 \
    mbedtls_mpi_cmp_int(X, c)

#define MPI_ECP_CMP(X, Y)                                                     \
    mbedtls_mpi_cmp_mpi(X, Y)

/* Needs f_rng, p_rng to be defined. */
#define MPI_ECP_RAND(X)                                                       \
    MBEDTLS_MPI_CHK(mbedtls_mpi_random((X), 2, &grp->P, f_rng, p_rng))

/* Conditional negation
 * Needs grp and a temporary MPI tmp to be defined. */
#define MPI_ECP_COND_NEG(X, cond)                                        \
    do                                                                     \
    {                                                                      \
        unsigned char nonzero = mbedtls_mpi_cmp_int((X), 0) != 0;        \
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&tmp, &grp->P, (X)));      \
        MBEDTLS_MPI_CHK(mbedtls_mpi_safe_cond_assign((X), &tmp,          \
                                                     nonzero & cond)); \
    } while (0)

#define MPI_ECP_NEG(X) MPI_ECP_COND_NEG((X), 1)

#define MPI_ECP_VALID(X)                      \
    ((X)->p != NULL)

#define MPI_ECP_COND_ASSIGN(X, Y, cond)       \
    MBEDTLS_MPI_CHK(mbedtls_mpi_safe_cond_assign((X), (Y), (cond)))

#define MPI_ECP_COND_SWAP(X, Y, cond)       \
    MBEDTLS_MPI_CHK(mbedtls_mpi_safe_cond_swap((X), (Y), (cond)))

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)

/*
 * Computes the right-hand side of the Short Weierstrass equation
 * RHS = X^3 + A X + B
 */
static int ecp_sw_rhs(const mbedtls_ecp_group *grp,
                      mbedtls_mpi *rhs,
                      const mbedtls_mpi *X)
{
    int ret;

    /* Compute X^3 + A X + B as X (X^2 + A) + B */
    MPI_ECP_SQR(rhs, X);

    /* Special case for A = -3 */
    if (mbedtls_ecp_group_a_is_minus_3(grp)) {
        MPI_ECP_SUB_INT(rhs, rhs, 3);
    } else {
        MPI_ECP_ADD(rhs, rhs, &grp->A);
    }

    MPI_ECP_MUL(rhs, rhs, X);
    MPI_ECP_ADD(rhs, rhs, &grp->B);

cleanup:
    return ret;
}

/*
 * Derive Y from X and a parity bit
 */
static int mbedtls_ecp_sw_derive_y(const mbedtls_ecp_group *grp,
                                   const mbedtls_mpi *X,
                                   mbedtls_mpi *Y,
                                   int parity_bit)
{
    /* w = y^2 = x^3 + ax + b
     * y = sqrt(w) = w^((p+1)/4) mod p   (for prime p where p = 3 mod 4)
     *
     * Note: this method for extracting square root does not validate that w
     * was indeed a square so this function will return garbage in Y if X
     * does not correspond to a point on the curve.
     */

    /* Check prerequisite p = 3 mod 4 */
    if (mbedtls_mpi_get_bit(&grp->P, 0) != 1 ||
        mbedtls_mpi_get_bit(&grp->P, 1) != 1) {
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }

    int ret;
    mbedtls_mpi exp;
    mbedtls_mpi_init(&exp);

    /* use Y to store intermediate result, actually w above */
    MBEDTLS_MPI_CHK(ecp_sw_rhs(grp, Y, X));

    /* w = y^2 */ /* Y contains y^2 intermediate result */
    /* exp = ((p+1)/4) */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&exp, &grp->P, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&exp, 2));
    /* sqrt(w) = w^((p+1)/4) mod p   (for prime p where p = 3 mod 4) */
    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(Y, Y /*y^2*/, &exp, &grp->P, NULL));

    /* check parity bit match or else invert Y */
    /* This quick inversion implementation is valid because Y != 0 for all
     * Short Weierstrass curves supported by mbedtls, as each supported curve
     * has an order that is a large prime, so each supported curve does not
     * have any point of order 2, and a point with Y == 0 would be of order 2 */
    if (mbedtls_mpi_get_bit(Y, 0) != parity_bit) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(Y, &grp->P, Y));
    }

cleanup:

    mbedtls_mpi_free(&exp);
    return ret;
}
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

#if defined(MBEDTLS_ECP_C)
#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
/*
 * For curves in short Weierstrass form, we do all the internal operations in
 * Jacobian coordinates.
 *
 * For multiplication, we'll use a comb method with countermeasures against
 * SPA, hence timing attacks.
 */

/*
 * Normalize jacobian coordinates so that Z == 0 || Z == 1  (GECC 3.2.1)
 * Cost: 1N := 1I + 3M + 1S
 */
static int ecp_normalize_jac(const mbedtls_ecp_group *grp, mbedtls_ecp_point *pt)
{
    if (MPI_ECP_CMP_INT(&pt->Z, 0) == 0) {
        return 0;
    }

#if defined(MBEDTLS_ECP_NORMALIZE_JAC_ALT)
    if (mbedtls_internal_ecp_grp_capable(grp)) {
        return mbedtls_internal_ecp_normalize_jac(grp, pt);
    }
#endif /* MBEDTLS_ECP_NORMALIZE_JAC_ALT */

#if defined(MBEDTLS_ECP_NO_FALLBACK) && defined(MBEDTLS_ECP_NORMALIZE_JAC_ALT)
    return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
#else
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi T;
    mbedtls_mpi_init(&T);

    MPI_ECP_INV(&T,       &pt->Z);            /* T   <-          1 / Z   */
    MPI_ECP_MUL(&pt->Y,   &pt->Y,     &T);    /* Y'  <- Y*T    = Y / Z   */
    MPI_ECP_SQR(&T,       &T);                /* T   <- T^2    = 1 / Z^2 */
    MPI_ECP_MUL(&pt->X,   &pt->X,     &T);    /* X   <- X  * T = X / Z^2 */
    MPI_ECP_MUL(&pt->Y,   &pt->Y,     &T);    /* Y'' <- Y' * T = Y / Z^3 */

    MPI_ECP_LSET(&pt->Z, 1);

cleanup:

    mbedtls_mpi_free(&T);

    return ret;
#endif /* !defined(MBEDTLS_ECP_NO_FALLBACK) || !defined(MBEDTLS_ECP_NORMALIZE_JAC_ALT) */
}

/*
 * Normalize jacobian coordinates of an array of (pointers to) points,
 * using Montgomery's trick to perform only one inversion mod P.
 * (See for example Cohen's "A Course in Computational Algebraic Number
 * Theory", Algorithm 10.3.4.)
 *
 * Warning: fails (returning an error) if one of the points is zero!
 * This should never happen, see choice of w in ecp_mul_comb().
 *
 * Cost: 1N(t) := 1I + (6t - 3)M + 1S
 */
static int ecp_normalize_jac_many(const mbedtls_ecp_group *grp,
                                  mbedtls_ecp_point *T[], size_t T_size)
{
    if (T_size < 2) {
        return ecp_normalize_jac(grp, *T);
    }

#if defined(MBEDTLS_ECP_NORMALIZE_JAC_MANY_ALT)
    if (mbedtls_internal_ecp_grp_capable(grp)) {
        return mbedtls_internal_ecp_normalize_jac_many(grp, T, T_size);
    }
#endif

#if defined(MBEDTLS_ECP_NO_FALLBACK) && defined(MBEDTLS_ECP_NORMALIZE_JAC_MANY_ALT)
    return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
#else
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i;
    mbedtls_mpi *c, t;

    if ((c = mbedtls_calloc(T_size, sizeof(mbedtls_mpi))) == NULL) {
        return MBEDTLS_ERR_ECP_ALLOC_FAILED;
    }

    mbedtls_mpi_init(&t);

    mpi_init_many(c, T_size);
    /*
     * c[i] = Z_0 * ... * Z_i,   i = 0,..,n := T_size-1
     */
    MPI_ECP_MOV(&c[0], &T[0]->Z);
    for (i = 1; i < T_size; i++) {
        MPI_ECP_MUL(&c[i], &c[i-1], &T[i]->Z);
    }

    /*
     * c[n] = 1 / (Z_0 * ... * Z_n) mod P
     */
    MPI_ECP_INV(&c[T_size-1], &c[T_size-1]);

    for (i = T_size - 1;; i--) {
        /* At the start of iteration i (note that i decrements), we have
         * - c[j] = Z_0 * .... * Z_j        for j  < i,
         * - c[j] = 1 / (Z_0 * .... * Z_j)  for j == i,
         *
         * This is maintained via
         * - c[i-1] <- c[i] * Z_i
         *
         * We also derive 1/Z_i = c[i] * c[i-1] for i>0 and use that
         * to do the actual normalization. For i==0, we already have
         * c[0] = 1 / Z_0.
         */

        if (i > 0) {
            /* Compute 1/Z_i and establish invariant for the next iteration. */
            MPI_ECP_MUL(&t,      &c[i], &c[i-1]);
            MPI_ECP_MUL(&c[i-1], &c[i], &T[i]->Z);
        } else {
            MPI_ECP_MOV(&t, &c[0]);
        }

        /* Now t holds 1 / Z_i; normalize as in ecp_normalize_jac() */
        MPI_ECP_MUL(&T[i]->Y, &T[i]->Y, &t);
        MPI_ECP_SQR(&t,       &t);
        MPI_ECP_MUL(&T[i]->X, &T[i]->X, &t);
        MPI_ECP_MUL(&T[i]->Y, &T[i]->Y, &t);

        /*
         * Post-precessing: reclaim some memory by shrinking coordinates
         * - not storing Z (always 1)
         * - shrinking other coordinates, but still keeping the same number of
         *   limbs as P, as otherwise it will too likely be regrown too fast.
         */
        MBEDTLS_MPI_CHK(mbedtls_mpi_shrink(&T[i]->X, grp->P.n));
        MBEDTLS_MPI_CHK(mbedtls_mpi_shrink(&T[i]->Y, grp->P.n));

        MPI_ECP_LSET(&T[i]->Z, 1);

        if (i == 0) {
            break;
        }
    }

cleanup:

    mbedtls_mpi_free(&t);
    mpi_free_many(c, T_size);
    mbedtls_free(c);

    return ret;
#endif /* !defined(MBEDTLS_ECP_NO_FALLBACK) || !defined(MBEDTLS_ECP_NORMALIZE_JAC_MANY_ALT) */
}

/*
 * Conditional point inversion: Q -> -Q = (Q.X, -Q.Y, Q.Z) without leak.
 * "inv" must be 0 (don't invert) or 1 (invert) or the result will be invalid
 */
static int ecp_safe_invert_jac(const mbedtls_ecp_group *grp,
                               mbedtls_ecp_point *Q,
                               unsigned char inv)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi tmp;
    mbedtls_mpi_init(&tmp);

    MPI_ECP_COND_NEG(&Q->Y, inv);

cleanup:
    mbedtls_mpi_free(&tmp);
    return ret;
}

/*
 * Point doubling R = 2 P, Jacobian coordinates
 *
 * Based on http://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#doubling-dbl-1998-cmo-2 .
 *
 * We follow the variable naming fairly closely. The formula variations that trade a MUL for a SQR
 * (plus a few ADDs) aren't useful as our bignum implementation doesn't distinguish squaring.
 *
 * Standard optimizations are applied when curve parameter A is one of { 0, -3 }.
 *
 * Cost: 1D := 3M + 4S          (A ==  0)
 *             4M + 4S          (A == -3)
 *             3M + 6S + 1a     otherwise
 */
static int ecp_double_jac(const mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                          const mbedtls_ecp_point *P,
                          mbedtls_mpi tmp[4])
{
#if defined(MBEDTLS_SELF_TEST)
    dbl_count++;
#endif

#if defined(MBEDTLS_ECP_DOUBLE_JAC_ALT)
    if (mbedtls_internal_ecp_grp_capable(grp)) {
        return mbedtls_internal_ecp_double_jac(grp, R, P);
    }
#endif /* MBEDTLS_ECP_DOUBLE_JAC_ALT */

#if defined(MBEDTLS_ECP_NO_FALLBACK) && defined(MBEDTLS_ECP_DOUBLE_JAC_ALT)
    return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
#else
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    /* Special case for A = -3 */
    if (mbedtls_ecp_group_a_is_minus_3(grp)) {
        /* tmp[0] <- M = 3(X + Z^2)(X - Z^2) */
        MPI_ECP_SQR(&tmp[1],  &P->Z);
        MPI_ECP_ADD(&tmp[2],  &P->X,  &tmp[1]);
        MPI_ECP_SUB(&tmp[3],  &P->X,  &tmp[1]);
        MPI_ECP_MUL(&tmp[1],  &tmp[2],     &tmp[3]);
        MPI_ECP_MUL_INT(&tmp[0],  &tmp[1],     3);
    } else {
        /* tmp[0] <- M = 3.X^2 + A.Z^4 */
        MPI_ECP_SQR(&tmp[1],  &P->X);
        MPI_ECP_MUL_INT(&tmp[0],  &tmp[1],  3);

        /* Optimize away for "koblitz" curves with A = 0 */
        if (MPI_ECP_CMP_INT(&grp->A, 0) != 0) {
            /* M += A.Z^4 */
            MPI_ECP_SQR(&tmp[1],  &P->Z);
            MPI_ECP_SQR(&tmp[2],  &tmp[1]);
            MPI_ECP_MUL(&tmp[1],  &tmp[2],     &grp->A);
            MPI_ECP_ADD(&tmp[0],  &tmp[0],     &tmp[1]);
        }
    }

    /* tmp[1] <- S = 4.X.Y^2 */
    MPI_ECP_SQR(&tmp[2],  &P->Y);
    MPI_ECP_SHIFT_L(&tmp[2],  1);
    MPI_ECP_MUL(&tmp[1],  &P->X, &tmp[2]);
    MPI_ECP_SHIFT_L(&tmp[1],  1);

    /* tmp[3] <- U = 8.Y^4 */
    MPI_ECP_SQR(&tmp[3],  &tmp[2]);
    MPI_ECP_SHIFT_L(&tmp[3],  1);

    /* tmp[2] <- T = M^2 - 2.S */
    MPI_ECP_SQR(&tmp[2],  &tmp[0]);
    MPI_ECP_SUB(&tmp[2],  &tmp[2], &tmp[1]);
    MPI_ECP_SUB(&tmp[2],  &tmp[2], &tmp[1]);

    /* tmp[1] <- S = M(S - T) - U */
    MPI_ECP_SUB(&tmp[1],  &tmp[1],     &tmp[2]);
    MPI_ECP_MUL(&tmp[1],  &tmp[1],     &tmp[0]);
    MPI_ECP_SUB(&tmp[1],  &tmp[1],     &tmp[3]);

    /* tmp[3] <- U = 2.Y.Z */
    MPI_ECP_MUL(&tmp[3],  &P->Y,  &P->Z);
    MPI_ECP_SHIFT_L(&tmp[3],  1);

    /* Store results */
    MPI_ECP_MOV(&R->X, &tmp[2]);
    MPI_ECP_MOV(&R->Y, &tmp[1]);
    MPI_ECP_MOV(&R->Z, &tmp[3]);

cleanup:

    return ret;
#endif /* !defined(MBEDTLS_ECP_NO_FALLBACK) || !defined(MBEDTLS_ECP_DOUBLE_JAC_ALT) */
}

/*
 * Addition: R = P + Q, mixed affine-Jacobian coordinates (GECC 3.22)
 *
 * The coordinates of Q must be normalized (= affine),
 * but those of P don't need to. R is not normalized.
 *
 * P,Q,R may alias, but only at the level of EC points: they must be either
 * equal as pointers, or disjoint (including the coordinate data buffers).
 * Fine-grained aliasing at the level of coordinates is not supported.
 *
 * Special cases: (1) P or Q is zero, (2) R is zero, (3) P == Q.
 * None of these cases can happen as intermediate step in ecp_mul_comb():
 * - at each step, P, Q and R are multiples of the base point, the factor
 *   being less than its order, so none of them is zero;
 * - Q is an odd multiple of the base point, P an even multiple,
 *   due to the choice of precomputed points in the modified comb method.
 * So branches for these cases do not leak secret information.
 *
 * Cost: 1A := 8M + 3S
 */
static int ecp_add_mixed(const mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                         const mbedtls_ecp_point *P, const mbedtls_ecp_point *Q,
                         mbedtls_mpi tmp[4])
{
#if defined(MBEDTLS_SELF_TEST)
    add_count++;
#endif

#if defined(MBEDTLS_ECP_ADD_MIXED_ALT)
    if (mbedtls_internal_ecp_grp_capable(grp)) {
        return mbedtls_internal_ecp_add_mixed(grp, R, P, Q);
    }
#endif /* MBEDTLS_ECP_ADD_MIXED_ALT */

#if defined(MBEDTLS_ECP_NO_FALLBACK) && defined(MBEDTLS_ECP_ADD_MIXED_ALT)
    return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
#else
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    /* NOTE: Aliasing between input and output is allowed, so one has to make
     *       sure that at the point X,Y,Z are written, {P,Q}->{X,Y,Z} are no
     *       longer read from. */
    mbedtls_mpi * const X = &R->X;
    mbedtls_mpi * const Y = &R->Y;
    mbedtls_mpi * const Z = &R->Z;

    if (!MPI_ECP_VALID(&Q->Z)) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /*
     * Trivial cases: P == 0 or Q == 0 (case 1)
     */
    if (MPI_ECP_CMP_INT(&P->Z, 0) == 0) {
        return mbedtls_ecp_copy(R, Q);
    }

    if (MPI_ECP_CMP_INT(&Q->Z, 0) == 0) {
        return mbedtls_ecp_copy(R, P);
    }

    /*
     * Make sure Q coordinates are normalized
     */
    if (MPI_ECP_CMP_INT(&Q->Z, 1) != 0) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    MPI_ECP_SQR(&tmp[0], &P->Z);
    MPI_ECP_MUL(&tmp[1], &tmp[0], &P->Z);
    MPI_ECP_MUL(&tmp[0], &tmp[0], &Q->X);
    MPI_ECP_MUL(&tmp[1], &tmp[1], &Q->Y);
    MPI_ECP_SUB(&tmp[0], &tmp[0], &P->X);
    MPI_ECP_SUB(&tmp[1], &tmp[1], &P->Y);

    /* Special cases (2) and (3) */
    if (MPI_ECP_CMP_INT(&tmp[0], 0) == 0) {
        if (MPI_ECP_CMP_INT(&tmp[1], 0) == 0) {
            ret = ecp_double_jac(grp, R, P, tmp);
            goto cleanup;
        } else {
            ret = mbedtls_ecp_set_zero(R);
            goto cleanup;
        }
    }

    /* {P,Q}->Z no longer used, so OK to write to Z even if there's aliasing. */
    MPI_ECP_MUL(Z,        &P->Z,    &tmp[0]);
    MPI_ECP_SQR(&tmp[2],  &tmp[0]);
    MPI_ECP_MUL(&tmp[3],  &tmp[2],  &tmp[0]);
    MPI_ECP_MUL(&tmp[2],  &tmp[2],  &P->X);

    MPI_ECP_MOV(&tmp[0], &tmp[2]);
    MPI_ECP_SHIFT_L(&tmp[0], 1);

    /* {P,Q}->X no longer used, so OK to write to X even if there's aliasing. */
    MPI_ECP_SQR(X,        &tmp[1]);
    MPI_ECP_SUB(X,        X,        &tmp[0]);
    MPI_ECP_SUB(X,        X,        &tmp[3]);
    MPI_ECP_SUB(&tmp[2],  &tmp[2],  X);
    MPI_ECP_MUL(&tmp[2],  &tmp[2],  &tmp[1]);
    MPI_ECP_MUL(&tmp[3],  &tmp[3],  &P->Y);
    /* {P,Q}->Y no longer used, so OK to write to Y even if there's aliasing. */
    MPI_ECP_SUB(Y,     &tmp[2],     &tmp[3]);

cleanup:

    return ret;
#endif /* !defined(MBEDTLS_ECP_NO_FALLBACK) || !defined(MBEDTLS_ECP_ADD_MIXED_ALT) */
}

/*
 * Randomize jacobian coordinates:
 * (X, Y, Z) -> (l^2 X, l^3 Y, l Z) for random l
 * This is sort of the reverse operation of ecp_normalize_jac().
 *
 * This countermeasure was first suggested in [2].
 */
static int ecp_randomize_jac(const mbedtls_ecp_group *grp, mbedtls_ecp_point *pt,
                             int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
#if defined(MBEDTLS_ECP_RANDOMIZE_JAC_ALT)
    if (mbedtls_internal_ecp_grp_capable(grp)) {
        return mbedtls_internal_ecp_randomize_jac(grp, pt, f_rng, p_rng);
    }
#endif /* MBEDTLS_ECP_RANDOMIZE_JAC_ALT */

#if defined(MBEDTLS_ECP_NO_FALLBACK) && defined(MBEDTLS_ECP_RANDOMIZE_JAC_ALT)
    return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
#else
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi l;

    mbedtls_mpi_init(&l);

    /* Generate l such that 1 < l < p */
    MPI_ECP_RAND(&l);

    /* Z' = l * Z */
    MPI_ECP_MUL(&pt->Z,   &pt->Z,     &l);

    /* Y' = l * Y */
    MPI_ECP_MUL(&pt->Y,   &pt->Y,     &l);

    /* X' = l^2 * X */
    MPI_ECP_SQR(&l,       &l);
    MPI_ECP_MUL(&pt->X,   &pt->X,     &l);

    /* Y'' = l^2 * Y' = l^3 * Y */
    MPI_ECP_MUL(&pt->Y,   &pt->Y,     &l);

cleanup:
    mbedtls_mpi_free(&l);

    if (ret == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) {
        ret = MBEDTLS_ERR_ECP_RANDOM_FAILED;
    }
    return ret;
#endif /* !defined(MBEDTLS_ECP_NO_FALLBACK) || !defined(MBEDTLS_ECP_RANDOMIZE_JAC_ALT) */
}

/*
 * Check and define parameters used by the comb method (see below for details)
 */
#if MBEDTLS_ECP_WINDOW_SIZE < 2 || MBEDTLS_ECP_WINDOW_SIZE > 7
#error "MBEDTLS_ECP_WINDOW_SIZE out of bounds"
#endif

/* d = ceil( n / w ) */
#define COMB_MAX_D      (MBEDTLS_ECP_MAX_BITS + 1) / 2

/* number of precomputed points */
#define COMB_MAX_PRE    (1 << (MBEDTLS_ECP_WINDOW_SIZE - 1))

/*
 * Compute the representation of m that will be used with our comb method.
 *
 * The basic comb method is described in GECC 3.44 for example. We use a
 * modified version that provides resistance to SPA by avoiding zero
 * digits in the representation as in [3]. We modify the method further by
 * requiring that all K_i be odd, which has the small cost that our
 * representation uses one more K_i, due to carries, but saves on the size of
 * the precomputed table.
 *
 * Summary of the comb method and its modifications:
 *
 * - The goal is to compute m*P for some w*d-bit integer m.
 *
 * - The basic comb method splits m into the w-bit integers
 *   x[0] .. x[d-1] where x[i] consists of the bits in m whose
 *   index has residue i modulo d, and computes m * P as
 *   S[x[0]] + 2 * S[x[1]] + .. + 2^(d-1) S[x[d-1]], where
 *   S[i_{w-1} .. i_0] := i_{w-1} 2^{(w-1)d} P + ... + i_1 2^d P + i_0 P.
 *
 * - If it happens that, say, x[i+1]=0 (=> S[x[i+1]]=0), one can replace the sum by
 *    .. + 2^{i-1} S[x[i-1]] - 2^i S[x[i]] + 2^{i+1} S[x[i]] + 2^{i+2} S[x[i+2]] ..,
 *   thereby successively converting it into a form where all summands
 *   are nonzero, at the cost of negative summands. This is the basic idea of [3].
 *
 * - More generally, even if x[i+1] != 0, we can first transform the sum as
 *   .. - 2^i S[x[i]] + 2^{i+1} ( S[x[i]] + S[x[i+1]] ) + 2^{i+2} S[x[i+2]] ..,
 *   and then replace S[x[i]] + S[x[i+1]] = S[x[i] ^ x[i+1]] + 2 S[x[i] & x[i+1]].
 *   Performing and iterating this procedure for those x[i] that are even
 *   (keeping track of carry), we can transform the original sum into one of the form
 *   S[x'[0]] +- 2 S[x'[1]] +- .. +- 2^{d-1} S[x'[d-1]] + 2^d S[x'[d]]
 *   with all x'[i] odd. It is therefore only necessary to know S at odd indices,
 *   which is why we are only computing half of it in the first place in
 *   ecp_precompute_comb and accessing it with index abs(i) / 2 in ecp_select_comb.
 *
 * - For the sake of compactness, only the seven low-order bits of x[i]
 *   are used to represent its absolute value (K_i in the paper), and the msb
 *   of x[i] encodes the sign (s_i in the paper): it is set if and only if
 *   if s_i == -1;
 *
 * Calling conventions:
 * - x is an array of size d + 1
 * - w is the size, ie number of teeth, of the comb, and must be between
 *   2 and 7 (in practice, between 2 and MBEDTLS_ECP_WINDOW_SIZE)
 * - m is the MPI, expected to be odd and such that bitlength(m) <= w * d
 *   (the result will be incorrect if these assumptions are not satisfied)
 */
static void ecp_comb_recode_core(unsigned char x[], size_t d,
                                 unsigned char w, const mbedtls_mpi *m)
{
    size_t i, j;
    unsigned char c, cc, adjust;

    memset(x, 0, d+1);

    /* First get the classical comb values (except for x_d = 0) */
    for (i = 0; i < d; i++) {
        for (j = 0; j < w; j++) {
            x[i] |= mbedtls_mpi_get_bit(m, i + d * j) << j;
        }
    }

    /* Now make sure x_1 .. x_d are odd */
    c = 0;
    for (i = 1; i <= d; i++) {
        /* Add carry and update it */
        cc   = x[i] & c;
        x[i] = x[i] ^ c;
        c = cc;

        /* Adjust if needed, avoiding branches */
        adjust = 1 - (x[i] & 0x01);
        c   |= x[i] & (x[i-1] * adjust);
        x[i] = x[i] ^ (x[i-1] * adjust);
        x[i-1] |= adjust << 7;
    }
}

/*
 * Precompute points for the adapted comb method
 *
 * Assumption: T must be able to hold 2^{w - 1} elements.
 *
 * Operation: If i = i_{w-1} ... i_1 is the binary representation of i,
 *            sets T[i] = i_{w-1} 2^{(w-1)d} P + ... + i_1 2^d P + P.
 *
 * Cost: d(w-1) D + (2^{w-1} - 1) A + 1 N(w-1) + 1 N(2^{w-1} - 1)
 *
 * Note: Even comb values (those where P would be omitted from the
 *       sum defining T[i] above) are not needed in our adaption
 *       the comb method. See ecp_comb_recode_core().
 *
 * This function currently works in four steps:
 * (1) [dbl]      Computation of intermediate T[i] for 2-power values of i
 * (2) [norm_dbl] Normalization of coordinates of these T[i]
 * (3) [add]      Computation of all T[i]
 * (4) [norm_add] Normalization of all T[i]
 *
 * Step 1 can be interrupted but not the others; together with the final
 * coordinate normalization they are the largest steps done at once, depending
 * on the window size. Here are operation counts for P-256:
 *
 * step     (2)     (3)     (4)
 * w = 5    142     165     208
 * w = 4    136      77     160
 * w = 3    130      33     136
 * w = 2    124      11     124
 *
 * So if ECC operations are blocking for too long even with a low max_ops
 * value, it's useful to set MBEDTLS_ECP_WINDOW_SIZE to a lower value in order
 * to minimize maximum blocking time.
 */
static int ecp_precompute_comb(const mbedtls_ecp_group *grp,
                               mbedtls_ecp_point T[], const mbedtls_ecp_point *P,
                               unsigned char w, size_t d,
                               mbedtls_ecp_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char i;
    size_t j = 0;
    const unsigned char T_size = 1U << (w - 1);
    mbedtls_ecp_point *cur, *TT[COMB_MAX_PRE - 1] = { NULL };

    mbedtls_mpi tmp[4];

    mpi_init_many(tmp, sizeof(tmp) / sizeof(mbedtls_mpi));

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL) {
        if (rs_ctx->rsm->state == ecp_rsm_pre_dbl) {
            goto dbl;
        }
        if (rs_ctx->rsm->state == ecp_rsm_pre_norm_dbl) {
            goto norm_dbl;
        }
        if (rs_ctx->rsm->state == ecp_rsm_pre_add) {
            goto add;
        }
        if (rs_ctx->rsm->state == ecp_rsm_pre_norm_add) {
            goto norm_add;
        }
    }
#else
    (void) rs_ctx;
#endif

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL) {
        rs_ctx->rsm->state = ecp_rsm_pre_dbl;

        /* initial state for the loop */
        rs_ctx->rsm->i = 0;
    }

dbl:
#endif
    /*
     * Set T[0] = P and
     * T[2^{l-1}] = 2^{dl} P for l = 1 .. w-1 (this is not the final value)
     */
    MBEDTLS_MPI_CHK(mbedtls_ecp_copy(&T[0], P));

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL && rs_ctx->rsm->i != 0) {
        j = rs_ctx->rsm->i;
    } else
#endif
    j = 0;

    for (; j < d * (w - 1); j++) {
        MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_DBL);

        i = 1U << (j / d);
        cur = T + i;

        if (j % d == 0) {
            MBEDTLS_MPI_CHK(mbedtls_ecp_copy(cur, T + (i >> 1)));
        }

        MBEDTLS_MPI_CHK(ecp_double_jac(grp, cur, cur, tmp));
    }

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL) {
        rs_ctx->rsm->state = ecp_rsm_pre_norm_dbl;
    }

norm_dbl:
#endif
    /*
     * Normalize current elements in T to allow them to be used in
     * ecp_add_mixed() below, which requires one normalized input.
     *
     * As T has holes, use an auxiliary array of pointers to elements in T.
     *
     */
    j = 0;
    for (i = 1; i < T_size; i <<= 1) {
        TT[j++] = T + i;
    }

    MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_INV + 6 * j - 2);

    MBEDTLS_MPI_CHK(ecp_normalize_jac_many(grp, TT, j));

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL) {
        rs_ctx->rsm->state = ecp_rsm_pre_add;
    }

add:
#endif
    /*
     * Compute the remaining ones using the minimal number of additions
     * Be careful to update T[2^l] only after using it!
     */
    MBEDTLS_ECP_BUDGET((T_size - 1) * MBEDTLS_ECP_OPS_ADD);

    for (i = 1; i < T_size; i <<= 1) {
        j = i;
        while (j--) {
            MBEDTLS_MPI_CHK(ecp_add_mixed(grp, &T[i + j], &T[j], &T[i], tmp));
        }
    }

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL) {
        rs_ctx->rsm->state = ecp_rsm_pre_norm_add;
    }

norm_add:
#endif
    /*
     * Normalize final elements in T. Even though there are no holes now, we
     * still need the auxiliary array for homogeneity with the previous
     * call. Also, skip T[0] which is already normalised, being a copy of P.
     */
    for (j = 0; j + 1 < T_size; j++) {
        TT[j] = T + j + 1;
    }

    MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_INV + 6 * j - 2);

    MBEDTLS_MPI_CHK(ecp_normalize_jac_many(grp, TT, j));

    /* Free Z coordinate (=1 after normalization) to save RAM.
     * This makes T[i] invalid as mbedtls_ecp_points, but this is OK
     * since from this point onwards, they are only accessed indirectly
     * via the getter function ecp_select_comb() which does set the
     * target's Z coordinate to 1. */
    for (i = 0; i < T_size; i++) {
        mbedtls_mpi_free(&T[i].Z);
    }

cleanup:

    mpi_free_many(tmp, sizeof(tmp) / sizeof(mbedtls_mpi));

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL &&
        ret == MBEDTLS_ERR_ECP_IN_PROGRESS) {
        if (rs_ctx->rsm->state == ecp_rsm_pre_dbl) {
            rs_ctx->rsm->i = j;
        }
    }
#endif

    return ret;
}

/*
 * Select precomputed point: R = sign(i) * T[ abs(i) / 2 ]
 *
 * See ecp_comb_recode_core() for background
 */
static int ecp_select_comb(const mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                           const mbedtls_ecp_point T[], unsigned char T_size,
                           unsigned char i)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char ii, j;

    /* Ignore the "sign" bit and scale down */
    ii =  (i & 0x7Fu) >> 1;

    /* Read the whole table to thwart cache-based timing attacks */
    for (j = 0; j < T_size; j++) {
        MPI_ECP_COND_ASSIGN(&R->X, &T[j].X, j == ii);
        MPI_ECP_COND_ASSIGN(&R->Y, &T[j].Y, j == ii);
    }

    /* Safely invert result if i is "negative" */
    MBEDTLS_MPI_CHK(ecp_safe_invert_jac(grp, R, i >> 7));

    MPI_ECP_LSET(&R->Z, 1);

cleanup:
    return ret;
}

/*
 * Core multiplication algorithm for the (modified) comb method.
 * This part is actually common with the basic comb method (GECC 3.44)
 *
 * Cost: d A + d D + 1 R
 */
static int ecp_mul_comb_core(const mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                             const mbedtls_ecp_point T[], unsigned char T_size,
                             const unsigned char x[], size_t d,
                             int (*f_rng)(void *, unsigned char *, size_t),
                             void *p_rng,
                             mbedtls_ecp_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_point Txi;
    mbedtls_mpi tmp[4];
    size_t i;

    mbedtls_ecp_point_init(&Txi);
    mpi_init_many(tmp, sizeof(tmp) / sizeof(mbedtls_mpi));

#if !defined(MBEDTLS_ECP_RESTARTABLE)
    (void) rs_ctx;
#endif

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL &&
        rs_ctx->rsm->state != ecp_rsm_comb_core) {
        rs_ctx->rsm->i = 0;
        rs_ctx->rsm->state = ecp_rsm_comb_core;
    }

    /* new 'if' instead of nested for the sake of the 'else' branch */
    if (rs_ctx != NULL && rs_ctx->rsm != NULL && rs_ctx->rsm->i != 0) {
        /* restore current index (R already pointing to rs_ctx->rsm->R) */
        i = rs_ctx->rsm->i;
    } else
#endif
    {
        /* Start with a non-zero point and randomize its coordinates */
        i = d;
        MBEDTLS_MPI_CHK(ecp_select_comb(grp, R, T, T_size, x[i]));
        if (f_rng != 0) {
            MBEDTLS_MPI_CHK(ecp_randomize_jac(grp, R, f_rng, p_rng));
        }
    }

    while (i != 0) {
        MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_DBL + MBEDTLS_ECP_OPS_ADD);
        --i;

        MBEDTLS_MPI_CHK(ecp_double_jac(grp, R, R, tmp));
        MBEDTLS_MPI_CHK(ecp_select_comb(grp, &Txi, T, T_size, x[i]));
        MBEDTLS_MPI_CHK(ecp_add_mixed(grp, R, R, &Txi, tmp));
    }

cleanup:

    mbedtls_ecp_point_free(&Txi);
    mpi_free_many(tmp, sizeof(tmp) / sizeof(mbedtls_mpi));

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL &&
        ret == MBEDTLS_ERR_ECP_IN_PROGRESS) {
        rs_ctx->rsm->i = i;
        /* no need to save R, already pointing to rs_ctx->rsm->R */
    }
#endif

    return ret;
}

/*
 * Recode the scalar to get constant-time comb multiplication
 *
 * As the actual scalar recoding needs an odd scalar as a starting point,
 * this wrapper ensures that by replacing m by N - m if necessary, and
 * informs the caller that the result of multiplication will be negated.
 *
 * This works because we only support large prime order for Short Weierstrass
 * curves, so N is always odd hence either m or N - m is.
 *
 * See ecp_comb_recode_core() for background.
 */
static int ecp_comb_recode_scalar(const mbedtls_ecp_group *grp,
                                  const mbedtls_mpi *m,
                                  unsigned char k[COMB_MAX_D + 1],
                                  size_t d,
                                  unsigned char w,
                                  unsigned char *parity_trick)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi M, mm;

    mbedtls_mpi_init(&M);
    mbedtls_mpi_init(&mm);

    /* N is always odd (see above), just make extra sure */
    if (mbedtls_mpi_get_bit(&grp->N, 0) != 1) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /* do we need the parity trick? */
    *parity_trick = (mbedtls_mpi_get_bit(m, 0) == 0);

    /* execute parity fix in constant time */
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&M, m));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&mm, &grp->N, m));
    MBEDTLS_MPI_CHK(mbedtls_mpi_safe_cond_assign(&M, &mm, *parity_trick));

    /* actual scalar recoding */
    ecp_comb_recode_core(k, d, w, &M);

cleanup:
    mbedtls_mpi_free(&mm);
    mbedtls_mpi_free(&M);

    return ret;
}

/*
 * Perform comb multiplication (for short Weierstrass curves)
 * once the auxiliary table has been pre-computed.
 *
 * Scalar recoding may use a parity trick that makes us compute -m * P,
 * if that is the case we'll need to recover m * P at the end.
 */
static int ecp_mul_comb_after_precomp(const mbedtls_ecp_group *grp,
                                      mbedtls_ecp_point *R,
                                      const mbedtls_mpi *m,
                                      const mbedtls_ecp_point *T,
                                      unsigned char T_size,
                                      unsigned char w,
                                      size_t d,
                                      int (*f_rng)(void *, unsigned char *, size_t),
                                      void *p_rng,
                                      mbedtls_ecp_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char parity_trick;
    unsigned char k[COMB_MAX_D + 1];
    mbedtls_ecp_point *RR = R;

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL) {
        RR = &rs_ctx->rsm->R;

        if (rs_ctx->rsm->state == ecp_rsm_final_norm) {
            goto final_norm;
        }
    }
#endif

    MBEDTLS_MPI_CHK(ecp_comb_recode_scalar(grp, m, k, d, w,
                                           &parity_trick));
    MBEDTLS_MPI_CHK(ecp_mul_comb_core(grp, RR, T, T_size, k, d,
                                      f_rng, p_rng, rs_ctx));
    MBEDTLS_MPI_CHK(ecp_safe_invert_jac(grp, RR, parity_trick));

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL) {
        rs_ctx->rsm->state = ecp_rsm_final_norm;
    }

final_norm:
    MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_INV);
#endif
    /*
     * Knowledge of the jacobian coordinates may leak the last few bits of the
     * scalar [1], and since our MPI implementation isn't constant-flow,
     * inversion (used for coordinate normalization) may leak the full value
     * of its input via side-channels [2].
     *
     * [1] https://eprint.iacr.org/2003/191
     * [2] https://eprint.iacr.org/2020/055
     *
     * Avoid the leak by randomizing coordinates before we normalize them.
     */
    if (f_rng != 0) {
        MBEDTLS_MPI_CHK(ecp_randomize_jac(grp, RR, f_rng, p_rng));
    }

    MBEDTLS_MPI_CHK(ecp_normalize_jac(grp, RR));

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL) {
        MBEDTLS_MPI_CHK(mbedtls_ecp_copy(R, RR));
    }
#endif

cleanup:
    return ret;
}

/*
 * Pick window size based on curve size and whether we optimize for base point
 */
static unsigned char ecp_pick_window_size(const mbedtls_ecp_group *grp,
                                          unsigned char p_eq_g)
{
    unsigned char w;

    /*
     * Minimize the number of multiplications, that is minimize
     * 10 * d * w + 18 * 2^(w-1) + 11 * d + 7 * w, with d = ceil( nbits / w )
     * (see costs of the various parts, with 1S = 1M)
     */
    w = grp->nbits >= 384 ? 5 : 4;

    /*
     * If P == G, pre-compute a bit more, since this may be re-used later.
     * Just adding one avoids upping the cost of the first mul too much,
     * and the memory cost too.
     */
    if (p_eq_g) {
        w++;
    }

    /*
     * If static comb table may not be used (!p_eq_g) or static comb table does
     * not exists, make sure w is within bounds.
     * (The last test is useful only for very small curves in the test suite.)
     *
     * The user reduces MBEDTLS_ECP_WINDOW_SIZE does not changes the size of
     * static comb table, because the size of static comb table is fixed when
     * it is generated.
     */
#if (MBEDTLS_ECP_WINDOW_SIZE < 6)
    if ((!p_eq_g || !ecp_group_is_static_comb_table(grp)) && w > MBEDTLS_ECP_WINDOW_SIZE) {
        w = MBEDTLS_ECP_WINDOW_SIZE;
    }
#endif
    if (w >= grp->nbits) {
        w = 2;
    }

    return w;
}

/*
 * Multiplication using the comb method - for curves in short Weierstrass form
 *
 * This function is mainly responsible for administrative work:
 * - managing the restart context if enabled
 * - managing the table of precomputed points (passed between the below two
 *   functions): allocation, computation, ownership transfer, freeing.
 *
 * It delegates the actual arithmetic work to:
 *      ecp_precompute_comb() and ecp_mul_comb_with_precomp()
 *
 * See comments on ecp_comb_recode_core() regarding the computation strategy.
 */
static int ecp_mul_comb(mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                        const mbedtls_mpi *m, const mbedtls_ecp_point *P,
                        int (*f_rng)(void *, unsigned char *, size_t),
                        void *p_rng,
                        mbedtls_ecp_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char w, p_eq_g, i;
    size_t d;
    unsigned char T_size = 0, T_ok = 0;
    mbedtls_ecp_point *T = NULL;

    ECP_RS_ENTER(rsm);

    /* Is P the base point ? */
#if MBEDTLS_ECP_FIXED_POINT_OPTIM == 1
    p_eq_g = (MPI_ECP_CMP(&P->Y, &grp->G.Y) == 0 &&
              MPI_ECP_CMP(&P->X, &grp->G.X) == 0);
#else
    p_eq_g = 0;
#endif

    /* Pick window size and deduce related sizes */
    w = ecp_pick_window_size(grp, p_eq_g);
    T_size = 1U << (w - 1);
    d = (grp->nbits + w - 1) / w;

    /* Pre-computed table: do we have it already for the base point? */
    if (p_eq_g && grp->T != NULL) {
        /* second pointer to the same table, will be deleted on exit */
        T = grp->T;
        T_ok = 1;
    } else
#if defined(MBEDTLS_ECP_RESTARTABLE)
    /* Pre-computed table: do we have one in progress? complete? */
    if (rs_ctx != NULL && rs_ctx->rsm != NULL && rs_ctx->rsm->T != NULL) {
        /* transfer ownership of T from rsm to local function */
        T = rs_ctx->rsm->T;
        rs_ctx->rsm->T = NULL;
        rs_ctx->rsm->T_size = 0;

        /* This effectively jumps to the call to mul_comb_after_precomp() */
        T_ok = rs_ctx->rsm->state >= ecp_rsm_comb_core;
    } else
#endif
    /* Allocate table if we didn't have any */
    {
        T = mbedtls_calloc(T_size, sizeof(mbedtls_ecp_point));
        if (T == NULL) {
            ret = MBEDTLS_ERR_ECP_ALLOC_FAILED;
            goto cleanup;
        }

        for (i = 0; i < T_size; i++) {
            mbedtls_ecp_point_init(&T[i]);
        }

        T_ok = 0;
    }

    /* Compute table (or finish computing it) if not done already */
    if (!T_ok) {
        MBEDTLS_MPI_CHK(ecp_precompute_comb(grp, T, P, w, d, rs_ctx));

        if (p_eq_g) {
            /* almost transfer ownership of T to the group, but keep a copy of
             * the pointer to use for calling the next function more easily */
            grp->T = T;
            grp->T_size = T_size;
        }
    }

    /* Actual comb multiplication using precomputed points */
    MBEDTLS_MPI_CHK(ecp_mul_comb_after_precomp(grp, R, m,
                                               T, T_size, w, d,
                                               f_rng, p_rng, rs_ctx));

cleanup:

    /* does T belong to the group? */
    if (T == grp->T) {
        T = NULL;
    }

    /* does T belong to the restart context? */
#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->rsm != NULL && ret == MBEDTLS_ERR_ECP_IN_PROGRESS && T != NULL) {
        /* transfer ownership of T from local function to rsm */
        rs_ctx->rsm->T_size = T_size;
        rs_ctx->rsm->T = T;
        T = NULL;
    }
#endif

    /* did T belong to us? then let's destroy it! */
    if (T != NULL) {
        for (i = 0; i < T_size; i++) {
            mbedtls_ecp_point_free(&T[i]);
        }
        mbedtls_free(T);
    }

    /* prevent caller from using invalid value */
    int should_free_R = (ret != 0);
#if defined(MBEDTLS_ECP_RESTARTABLE)
    /* don't free R while in progress in case R == P */
    if (ret == MBEDTLS_ERR_ECP_IN_PROGRESS) {
        should_free_R = 0;
    }
#endif
    if (should_free_R) {
        mbedtls_ecp_point_free(R);
    }

    ECP_RS_LEAVE(rsm);

    return ret;
}

#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
/*
 * For Montgomery curves, we do all the internal arithmetic in projective
 * coordinates. Import/export of points uses only the x coordinates, which is
 * internally represented as X / Z.
 *
 * For scalar multiplication, we'll use a Montgomery ladder.
 */

/*
 * Normalize Montgomery x/z coordinates: X = X/Z, Z = 1
 * Cost: 1M + 1I
 */
static int ecp_normalize_mxz(const mbedtls_ecp_group *grp, mbedtls_ecp_point *P)
{
#if defined(MBEDTLS_ECP_NORMALIZE_MXZ_ALT)
    if (mbedtls_internal_ecp_grp_capable(grp)) {
        return mbedtls_internal_ecp_normalize_mxz(grp, P);
    }
#endif /* MBEDTLS_ECP_NORMALIZE_MXZ_ALT */

#if defined(MBEDTLS_ECP_NO_FALLBACK) && defined(MBEDTLS_ECP_NORMALIZE_MXZ_ALT)
    return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
#else
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MPI_ECP_INV(&P->Z, &P->Z);
    MPI_ECP_MUL(&P->X, &P->X, &P->Z);
    MPI_ECP_LSET(&P->Z, 1);

cleanup:
    return ret;
#endif /* !defined(MBEDTLS_ECP_NO_FALLBACK) || !defined(MBEDTLS_ECP_NORMALIZE_MXZ_ALT) */
}

/*
 * Randomize projective x/z coordinates:
 * (X, Z) -> (l X, l Z) for random l
 * This is sort of the reverse operation of ecp_normalize_mxz().
 *
 * This countermeasure was first suggested in [2].
 * Cost: 2M
 */
static int ecp_randomize_mxz(const mbedtls_ecp_group *grp, mbedtls_ecp_point *P,
                             int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
#if defined(MBEDTLS_ECP_RANDOMIZE_MXZ_ALT)
    if (mbedtls_internal_ecp_grp_capable(grp)) {
        return mbedtls_internal_ecp_randomize_mxz(grp, P, f_rng, p_rng);
    }
#endif /* MBEDTLS_ECP_RANDOMIZE_MXZ_ALT */

#if defined(MBEDTLS_ECP_NO_FALLBACK) && defined(MBEDTLS_ECP_RANDOMIZE_MXZ_ALT)
    return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
#else
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi l;
    mbedtls_mpi_init(&l);

    /* Generate l such that 1 < l < p */
    MPI_ECP_RAND(&l);

    MPI_ECP_MUL(&P->X, &P->X, &l);
    MPI_ECP_MUL(&P->Z, &P->Z, &l);

cleanup:
    mbedtls_mpi_free(&l);

    if (ret == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) {
        ret = MBEDTLS_ERR_ECP_RANDOM_FAILED;
    }
    return ret;
#endif /* !defined(MBEDTLS_ECP_NO_FALLBACK) || !defined(MBEDTLS_ECP_RANDOMIZE_MXZ_ALT) */
}

/*
 * Double-and-add: R = 2P, S = P + Q, with d = X(P - Q),
 * for Montgomery curves in x/z coordinates.
 *
 * http://www.hyperelliptic.org/EFD/g1p/auto-code/montgom/xz/ladder/mladd-1987-m.op3
 * with
 * d =  X1
 * P = (X2, Z2)
 * Q = (X3, Z3)
 * R = (X4, Z4)
 * S = (X5, Z5)
 * and eliminating temporary variables tO, ..., t4.
 *
 * Cost: 5M + 4S
 */
static int ecp_double_add_mxz(const mbedtls_ecp_group *grp,
                              mbedtls_ecp_point *R, mbedtls_ecp_point *S,
                              const mbedtls_ecp_point *P, const mbedtls_ecp_point *Q,
                              const mbedtls_mpi *d,
                              mbedtls_mpi T[4])
{
#if defined(MBEDTLS_ECP_DOUBLE_ADD_MXZ_ALT)
    if (mbedtls_internal_ecp_grp_capable(grp)) {
        return mbedtls_internal_ecp_double_add_mxz(grp, R, S, P, Q, d);
    }
#endif /* MBEDTLS_ECP_DOUBLE_ADD_MXZ_ALT */

#if defined(MBEDTLS_ECP_NO_FALLBACK) && defined(MBEDTLS_ECP_DOUBLE_ADD_MXZ_ALT)
    return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
#else
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MPI_ECP_ADD(&T[0], &P->X,   &P->Z);   /* Pp := PX + PZ                    */
    MPI_ECP_SUB(&T[1], &P->X,   &P->Z);   /* Pm := PX - PZ                    */
    MPI_ECP_ADD(&T[2], &Q->X,   &Q->Z);   /* Qp := QX + XZ                    */
    MPI_ECP_SUB(&T[3], &Q->X,   &Q->Z);   /* Qm := QX - QZ                    */
    MPI_ECP_MUL(&T[3], &T[3],   &T[0]);   /* Qm * Pp                          */
    MPI_ECP_MUL(&T[2], &T[2],   &T[1]);   /* Qp * Pm                          */
    MPI_ECP_SQR(&T[0], &T[0]);            /* Pp^2                             */
    MPI_ECP_SQR(&T[1], &T[1]);            /* Pm^2                             */
    MPI_ECP_MUL(&R->X, &T[0],   &T[1]);   /* Pp^2 * Pm^2                      */
    MPI_ECP_SUB(&T[0], &T[0],   &T[1]);   /* Pp^2 - Pm^2                      */
    MPI_ECP_MUL(&R->Z, &grp->A, &T[0]);   /* A * (Pp^2 - Pm^2)                */
    MPI_ECP_ADD(&R->Z, &T[1],   &R->Z);   /* [ A * (Pp^2-Pm^2) ] + Pm^2       */
    MPI_ECP_ADD(&S->X, &T[3],   &T[2]);   /* Qm*Pp + Qp*Pm                    */
    MPI_ECP_SQR(&S->X, &S->X);            /* (Qm*Pp + Qp*Pm)^2                */
    MPI_ECP_SUB(&S->Z, &T[3],   &T[2]);   /* Qm*Pp - Qp*Pm                    */
    MPI_ECP_SQR(&S->Z, &S->Z);            /* (Qm*Pp - Qp*Pm)^2                */
    MPI_ECP_MUL(&S->Z, d,       &S->Z);   /* d * ( Qm*Pp - Qp*Pm )^2          */
    MPI_ECP_MUL(&R->Z, &T[0],   &R->Z);   /* [A*(Pp^2-Pm^2)+Pm^2]*(Pp^2-Pm^2) */

cleanup:

    return ret;
#endif /* !defined(MBEDTLS_ECP_NO_FALLBACK) || !defined(MBEDTLS_ECP_DOUBLE_ADD_MXZ_ALT) */
}

/*
 * Multiplication with Montgomery ladder in x/z coordinates,
 * for curves in Montgomery form
 */
static int ecp_mul_mxz(mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                       const mbedtls_mpi *m, const mbedtls_ecp_point *P,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t i;
    unsigned char b;
    mbedtls_ecp_point RP;
    mbedtls_mpi PX;
    mbedtls_mpi tmp[4];
    mbedtls_ecp_point_init(&RP); mbedtls_mpi_init(&PX);

    mpi_init_many(tmp, sizeof(tmp) / sizeof(mbedtls_mpi));

    if (f_rng == NULL) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /* Save PX and read from P before writing to R, in case P == R */
    MPI_ECP_MOV(&PX, &P->X);
    MBEDTLS_MPI_CHK(mbedtls_ecp_copy(&RP, P));

    /* Set R to zero in modified x/z coordinates */
    MPI_ECP_LSET(&R->X, 1);
    MPI_ECP_LSET(&R->Z, 0);
    mbedtls_mpi_free(&R->Y);

    /* RP.X might be slightly larger than P, so reduce it */
    MOD_ADD(&RP.X);

    /* Randomize coordinates of the starting point */
    MBEDTLS_MPI_CHK(ecp_randomize_mxz(grp, &RP, f_rng, p_rng));

    /* Loop invariant: R = result so far, RP = R + P */
    i = grp->nbits + 1; /* one past the (zero-based) required msb for private keys */
    while (i-- > 0) {
        b = mbedtls_mpi_get_bit(m, i);
        /*
         *  if (b) R = 2R + P else R = 2R,
         * which is:
         *  if (b) double_add( RP, R, RP, R )
         *  else   double_add( R, RP, R, RP )
         * but using safe conditional swaps to avoid leaks
         */
        MPI_ECP_COND_SWAP(&R->X, &RP.X, b);
        MPI_ECP_COND_SWAP(&R->Z, &RP.Z, b);
        MBEDTLS_MPI_CHK(ecp_double_add_mxz(grp, R, &RP, R, &RP, &PX, tmp));
        MPI_ECP_COND_SWAP(&R->X, &RP.X, b);
        MPI_ECP_COND_SWAP(&R->Z, &RP.Z, b);
    }

    /*
     * Knowledge of the projective coordinates may leak the last few bits of the
     * scalar [1], and since our MPI implementation isn't constant-flow,
     * inversion (used for coordinate normalization) may leak the full value
     * of its input via side-channels [2].
     *
     * [1] https://eprint.iacr.org/2003/191
     * [2] https://eprint.iacr.org/2020/055
     *
     * Avoid the leak by randomizing coordinates before we normalize them.
     */
    MBEDTLS_MPI_CHK(ecp_randomize_mxz(grp, R, f_rng, p_rng));
    MBEDTLS_MPI_CHK(ecp_normalize_mxz(grp, R));

cleanup:
    mbedtls_ecp_point_free(&RP); mbedtls_mpi_free(&PX);

    mpi_free_many(tmp, sizeof(tmp) / sizeof(mbedtls_mpi));
    return ret;
}

#endif /* MBEDTLS_ECP_MONTGOMERY_ENABLED */

/*
 * Restartable multiplication R = m * P
 *
 * This internal function can be called without an RNG in case where we know
 * the inputs are not sensitive.
 */
static int ecp_mul_restartable_internal(mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                                        const mbedtls_mpi *m, const mbedtls_ecp_point *P,
                                        int (*f_rng)(void *, unsigned char *, size_t), void *p_rng,
                                        mbedtls_ecp_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
#if defined(MBEDTLS_ECP_INTERNAL_ALT)
    char is_grp_capable = 0;
#endif

#if defined(MBEDTLS_ECP_RESTARTABLE)
    /* reset ops count for this call if top-level */
    if (rs_ctx != NULL && rs_ctx->depth++ == 0) {
        rs_ctx->ops_done = 0;
    }
#else
    (void) rs_ctx;
#endif

#if defined(MBEDTLS_ECP_INTERNAL_ALT)
    if ((is_grp_capable = mbedtls_internal_ecp_grp_capable(grp))) {
        MBEDTLS_MPI_CHK(mbedtls_internal_ecp_init(grp));
    }
#endif /* MBEDTLS_ECP_INTERNAL_ALT */

    int restarting = 0;
#if defined(MBEDTLS_ECP_RESTARTABLE)
    restarting = (rs_ctx != NULL && rs_ctx->rsm != NULL);
#endif
    /* skip argument check when restarting */
    if (!restarting) {
        /* check_privkey is free */
        MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_CHK);

        /* Common sanity checks */
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_privkey(grp, m));
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(grp, P));
    }

    ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_MONTGOMERY) {
        MBEDTLS_MPI_CHK(ecp_mul_mxz(grp, R, m, P, f_rng, p_rng));
    }
#endif
#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS) {
        MBEDTLS_MPI_CHK(ecp_mul_comb(grp, R, m, P, f_rng, p_rng, rs_ctx));
    }
#endif

cleanup:

#if defined(MBEDTLS_ECP_INTERNAL_ALT)
    if (is_grp_capable) {
        mbedtls_internal_ecp_free(grp);
    }
#endif /* MBEDTLS_ECP_INTERNAL_ALT */

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL) {
        rs_ctx->depth--;
    }
#endif

    return ret;
}

/*
 * Restartable multiplication R = m * P
 */
int mbedtls_ecp_mul_restartable(mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                                const mbedtls_mpi *m, const mbedtls_ecp_point *P,
                                int (*f_rng)(void *, unsigned char *, size_t), void *p_rng,
                                mbedtls_ecp_restart_ctx *rs_ctx)
{
    if (f_rng == NULL) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    return ecp_mul_restartable_internal(grp, R, m, P, f_rng, p_rng, rs_ctx);
}

/*
 * Multiplication R = m * P
 */
int mbedtls_ecp_mul(mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                    const mbedtls_mpi *m, const mbedtls_ecp_point *P,
                    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    return mbedtls_ecp_mul_restartable(grp, R, m, P, f_rng, p_rng, NULL);
}
#endif /* MBEDTLS_ECP_C */

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
/*
 * Check that an affine point is valid as a public key,
 * short weierstrass curves (SEC1 3.2.3.1)
 */
static int ecp_check_pubkey_sw(const mbedtls_ecp_group *grp, const mbedtls_ecp_point *pt)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi YY, RHS;

    /* pt coordinates must be normalized for our checks */
    if (mbedtls_mpi_cmp_int(&pt->X, 0) < 0 ||
        mbedtls_mpi_cmp_int(&pt->Y, 0) < 0 ||
        mbedtls_mpi_cmp_mpi(&pt->X, &grp->P) >= 0 ||
        mbedtls_mpi_cmp_mpi(&pt->Y, &grp->P) >= 0) {
        return MBEDTLS_ERR_ECP_INVALID_KEY;
    }

    mbedtls_mpi_init(&YY); mbedtls_mpi_init(&RHS);

    /*
     * YY = Y^2
     * RHS = X^3 + A X + B
     */
    MPI_ECP_SQR(&YY,  &pt->Y);
    MBEDTLS_MPI_CHK(ecp_sw_rhs(grp, &RHS, &pt->X));

    if (MPI_ECP_CMP(&YY, &RHS) != 0) {
        ret = MBEDTLS_ERR_ECP_INVALID_KEY;
    }

cleanup:

    mbedtls_mpi_free(&YY); mbedtls_mpi_free(&RHS);

    return ret;
}
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

#if defined(MBEDTLS_ECP_C)
#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
/*
 * R = m * P with shortcuts for m == 0, m == 1 and m == -1
 * NOT constant-time - ONLY for short Weierstrass!
 */
static int mbedtls_ecp_mul_shortcuts(mbedtls_ecp_group *grp,
                                     mbedtls_ecp_point *R,
                                     const mbedtls_mpi *m,
                                     const mbedtls_ecp_point *P,
                                     mbedtls_ecp_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi tmp;
    mbedtls_mpi_init(&tmp);

    if (mbedtls_mpi_cmp_int(m, 0) == 0) {
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(grp, P));
        MBEDTLS_MPI_CHK(mbedtls_ecp_set_zero(R));
    } else if (mbedtls_mpi_cmp_int(m, 1) == 0) {
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(grp, P));
        MBEDTLS_MPI_CHK(mbedtls_ecp_copy(R, P));
    } else if (mbedtls_mpi_cmp_int(m, -1) == 0) {
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_pubkey(grp, P));
        MBEDTLS_MPI_CHK(mbedtls_ecp_copy(R, P));
        MPI_ECP_NEG(&R->Y);
    } else {
        MBEDTLS_MPI_CHK(ecp_mul_restartable_internal(grp, R, m, P,
                                                     NULL, NULL, rs_ctx));
    }

cleanup:
    mbedtls_mpi_free(&tmp);

    return ret;
}

/*
 * Restartable linear combination
 * NOT constant-time
 */
int mbedtls_ecp_muladd_restartable(
    mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
    const mbedtls_mpi *m, const mbedtls_ecp_point *P,
    const mbedtls_mpi *n, const mbedtls_ecp_point *Q,
    mbedtls_ecp_restart_ctx *rs_ctx)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_point mP;
    mbedtls_ecp_point *pmP = &mP;
    mbedtls_ecp_point *pR = R;
    mbedtls_mpi tmp[4];
#if defined(MBEDTLS_ECP_INTERNAL_ALT)
    char is_grp_capable = 0;
#endif
    if (mbedtls_ecp_get_type(grp) != MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS) {
        return MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
    }

    mbedtls_ecp_point_init(&mP);
    mpi_init_many(tmp, sizeof(tmp) / sizeof(mbedtls_mpi));

    ECP_RS_ENTER(ma);

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->ma != NULL) {
        /* redirect intermediate results to restart context */
        pmP = &rs_ctx->ma->mP;
        pR  = &rs_ctx->ma->R;

        /* jump to next operation */
        if (rs_ctx->ma->state == ecp_rsma_mul2) {
            goto mul2;
        }
        if (rs_ctx->ma->state == ecp_rsma_add) {
            goto add;
        }
        if (rs_ctx->ma->state == ecp_rsma_norm) {
            goto norm;
        }
    }
#endif /* MBEDTLS_ECP_RESTARTABLE */

    MBEDTLS_MPI_CHK(mbedtls_ecp_mul_shortcuts(grp, pmP, m, P, rs_ctx));
#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->ma != NULL) {
        rs_ctx->ma->state = ecp_rsma_mul2;
    }

mul2:
#endif
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul_shortcuts(grp, pR,  n, Q, rs_ctx));

#if defined(MBEDTLS_ECP_INTERNAL_ALT)
    if ((is_grp_capable = mbedtls_internal_ecp_grp_capable(grp))) {
        MBEDTLS_MPI_CHK(mbedtls_internal_ecp_init(grp));
    }
#endif /* MBEDTLS_ECP_INTERNAL_ALT */

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->ma != NULL) {
        rs_ctx->ma->state = ecp_rsma_add;
    }

add:
#endif
    MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_ADD);
    MBEDTLS_MPI_CHK(ecp_add_mixed(grp, pR, pmP, pR, tmp));
#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->ma != NULL) {
        rs_ctx->ma->state = ecp_rsma_norm;
    }

norm:
#endif
    MBEDTLS_ECP_BUDGET(MBEDTLS_ECP_OPS_INV);
    MBEDTLS_MPI_CHK(ecp_normalize_jac(grp, pR));

#if defined(MBEDTLS_ECP_RESTARTABLE)
    if (rs_ctx != NULL && rs_ctx->ma != NULL) {
        MBEDTLS_MPI_CHK(mbedtls_ecp_copy(R, pR));
    }
#endif

cleanup:

    mpi_free_many(tmp, sizeof(tmp) / sizeof(mbedtls_mpi));

#if defined(MBEDTLS_ECP_INTERNAL_ALT)
    if (is_grp_capable) {
        mbedtls_internal_ecp_free(grp);
    }
#endif /* MBEDTLS_ECP_INTERNAL_ALT */

    mbedtls_ecp_point_free(&mP);

    ECP_RS_LEAVE(ma);

    return ret;
}

/*
 * Linear combination
 * NOT constant-time
 */
int mbedtls_ecp_muladd(mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                       const mbedtls_mpi *m, const mbedtls_ecp_point *P,
                       const mbedtls_mpi *n, const mbedtls_ecp_point *Q)
{
    return mbedtls_ecp_muladd_restartable(grp, R, m, P, n, Q, NULL);
}
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */
#endif /* MBEDTLS_ECP_C */

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
#if defined(MBEDTLS_ECP_DP_CURVE25519_ENABLED)
#define ECP_MPI_INIT(_p, _n) { .p = (mbedtls_mpi_uint *) (_p), .s = 1, .n = (_n) }
#define ECP_MPI_INIT_ARRAY(x)   \
    ECP_MPI_INIT(x, sizeof(x) / sizeof(mbedtls_mpi_uint))
/*
 * Constants for the two points other than 0, 1, -1 (mod p) in
 * https://cr.yp.to/ecdh.html#validate
 * See ecp_check_pubkey_x25519().
 */
static const mbedtls_mpi_uint x25519_bad_point_1[] = {
    MBEDTLS_BYTES_TO_T_UINT_8(0xe0, 0xeb, 0x7a, 0x7c, 0x3b, 0x41, 0xb8, 0xae),
    MBEDTLS_BYTES_TO_T_UINT_8(0x16, 0x56, 0xe3, 0xfa, 0xf1, 0x9f, 0xc4, 0x6a),
    MBEDTLS_BYTES_TO_T_UINT_8(0xda, 0x09, 0x8d, 0xeb, 0x9c, 0x32, 0xb1, 0xfd),
    MBEDTLS_BYTES_TO_T_UINT_8(0x86, 0x62, 0x05, 0x16, 0x5f, 0x49, 0xb8, 0x00),
};
static const mbedtls_mpi_uint x25519_bad_point_2[] = {
    MBEDTLS_BYTES_TO_T_UINT_8(0x5f, 0x9c, 0x95, 0xbc, 0xa3, 0x50, 0x8c, 0x24),
    MBEDTLS_BYTES_TO_T_UINT_8(0xb1, 0xd0, 0xb1, 0x55, 0x9c, 0x83, 0xef, 0x5b),
    MBEDTLS_BYTES_TO_T_UINT_8(0x04, 0x44, 0x5c, 0xc4, 0x58, 0x1c, 0x8e, 0x86),
    MBEDTLS_BYTES_TO_T_UINT_8(0xd8, 0x22, 0x4e, 0xdd, 0xd0, 0x9f, 0x11, 0x57),
};
static const mbedtls_mpi ecp_x25519_bad_point_1 = ECP_MPI_INIT_ARRAY(
    x25519_bad_point_1);
static const mbedtls_mpi ecp_x25519_bad_point_2 = ECP_MPI_INIT_ARRAY(
    x25519_bad_point_2);
#endif /* MBEDTLS_ECP_DP_CURVE25519_ENABLED */

/*
 * Check that the input point is not one of the low-order points.
 * This is recommended by the "May the Fourth" paper:
 * https://eprint.iacr.org/2017/806.pdf
 * Those points are never sent by an honest peer.
 */
static int ecp_check_bad_points_mx(const mbedtls_mpi *X, const mbedtls_mpi *P,
                                   const mbedtls_ecp_group_id grp_id)
{
    int ret;
    mbedtls_mpi XmP;

    mbedtls_mpi_init(&XmP);

    /* Reduce X mod P so that we only need to check values less than P.
     * We know X < 2^256 so we can proceed by subtraction. */
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&XmP, X));
    while (mbedtls_mpi_cmp_mpi(&XmP, P) >= 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&XmP, &XmP, P));
    }

    /* Check against the known bad values that are less than P. For Curve448
     * these are 0, 1 and -1. For Curve25519 we check the values less than P
     * from the following list: https://cr.yp.to/ecdh.html#validate */
    if (mbedtls_mpi_cmp_int(&XmP, 1) <= 0) {  /* takes care of 0 and 1 */
        ret = MBEDTLS_ERR_ECP_INVALID_KEY;
        goto cleanup;
    }

#if defined(MBEDTLS_ECP_DP_CURVE25519_ENABLED)
    if (grp_id == MBEDTLS_ECP_DP_CURVE25519) {
        if (mbedtls_mpi_cmp_mpi(&XmP, &ecp_x25519_bad_point_1) == 0) {
            ret = MBEDTLS_ERR_ECP_INVALID_KEY;
            goto cleanup;
        }

        if (mbedtls_mpi_cmp_mpi(&XmP, &ecp_x25519_bad_point_2) == 0) {
            ret = MBEDTLS_ERR_ECP_INVALID_KEY;
            goto cleanup;
        }
    }
#else
    (void) grp_id;
#endif

    /* Final check: check if XmP + 1 is P (final because it changes XmP!) */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&XmP, &XmP, 1));
    if (mbedtls_mpi_cmp_mpi(&XmP, P) == 0) {
        ret = MBEDTLS_ERR_ECP_INVALID_KEY;
        goto cleanup;
    }

    ret = 0;

cleanup:
    mbedtls_mpi_free(&XmP);

    return ret;
}

/*
 * Check validity of a public key for Montgomery curves with x-only schemes
 */
static int ecp_check_pubkey_mx(const mbedtls_ecp_group *grp, const mbedtls_ecp_point *pt)
{
    /* [Curve25519 p. 5] Just check X is the correct number of bytes */
    /* Allow any public value, if it's too big then we'll just reduce it mod p
     * (RFC 7748 sec. 5 para. 3). */
    if (mbedtls_mpi_size(&pt->X) > (grp->nbits + 7) / 8) {
        return MBEDTLS_ERR_ECP_INVALID_KEY;
    }

    /* Implicit in all standards (as they don't consider negative numbers):
     * X must be non-negative. This is normally ensured by the way it's
     * encoded for transmission, but let's be extra sure. */
    if (mbedtls_mpi_cmp_int(&pt->X, 0) < 0) {
        return MBEDTLS_ERR_ECP_INVALID_KEY;
    }

    return ecp_check_bad_points_mx(&pt->X, &grp->P, grp->id);
}
#endif /* MBEDTLS_ECP_MONTGOMERY_ENABLED */

/*
 * Check that a point is valid as a public key
 */
int mbedtls_ecp_check_pubkey(const mbedtls_ecp_group *grp,
                             const mbedtls_ecp_point *pt)
{
    /* Must use affine coordinates */
    if (mbedtls_mpi_cmp_int(&pt->Z, 1) != 0) {
        return MBEDTLS_ERR_ECP_INVALID_KEY;
    }

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_MONTGOMERY) {
        return ecp_check_pubkey_mx(grp, pt);
    }
#endif
#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS) {
        return ecp_check_pubkey_sw(grp, pt);
    }
#endif
    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
}

/*
 * Check that an mbedtls_mpi is valid as a private key
 */
int mbedtls_ecp_check_privkey(const mbedtls_ecp_group *grp,
                              const mbedtls_mpi *d)
{
#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_MONTGOMERY) {
        /* see RFC 7748 sec. 5 para. 5 */
        if (mbedtls_mpi_get_bit(d, 0) != 0 ||
            mbedtls_mpi_get_bit(d, 1) != 0 ||
            mbedtls_mpi_bitlen(d) != grp->nbits + 1) {  /* mbedtls_mpi_bitlen is one-based! */
            return MBEDTLS_ERR_ECP_INVALID_KEY;
        }

        /* see [Curve25519] page 5 */
        if (grp->nbits == 254 && mbedtls_mpi_get_bit(d, 2) != 0) {
            return MBEDTLS_ERR_ECP_INVALID_KEY;
        }

        return 0;
    }
#endif /* MBEDTLS_ECP_MONTGOMERY_ENABLED */
#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS) {
        /* see SEC1 3.2 */
        if (mbedtls_mpi_cmp_int(d, 1) < 0 ||
            mbedtls_mpi_cmp_mpi(d, &grp->N) >= 0) {
            return MBEDTLS_ERR_ECP_INVALID_KEY;
        } else {
            return 0;
        }
    }
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
}

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
MBEDTLS_STATIC_TESTABLE
int mbedtls_ecp_gen_privkey_mx(size_t high_bit,
                               mbedtls_mpi *d,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng)
{
    int ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    size_t n_random_bytes = high_bit / 8 + 1;

    /* [Curve25519] page 5 */
    /* Generate a (high_bit+1)-bit random number by generating just enough
     * random bytes, then shifting out extra bits from the top (necessary
     * when (high_bit+1) is not a multiple of 8). */
    MBEDTLS_MPI_CHK(mbedtls_mpi_fill_random(d, n_random_bytes,
                                            f_rng, p_rng));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(d, 8 * n_random_bytes - high_bit - 1));

    MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(d, high_bit, 1));

    /* Make sure the last two bits are unset for Curve448, three bits for
       Curve25519 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(d, 0, 0));
    MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(d, 1, 0));
    if (high_bit == 254) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(d, 2, 0));
    }

cleanup:
    return ret;
}
#endif /* MBEDTLS_ECP_MONTGOMERY_ENABLED */

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
static int mbedtls_ecp_gen_privkey_sw(
    const mbedtls_mpi *N, mbedtls_mpi *d,
    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret = mbedtls_mpi_random(d, 1, N, f_rng, p_rng);
    switch (ret) {
        case MBEDTLS_ERR_MPI_NOT_ACCEPTABLE:
            return MBEDTLS_ERR_ECP_RANDOM_FAILED;
        default:
            return ret;
    }
}
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

/*
 * Generate a private key
 */
int mbedtls_ecp_gen_privkey(const mbedtls_ecp_group *grp,
                            mbedtls_mpi *d,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_MONTGOMERY) {
        return mbedtls_ecp_gen_privkey_mx(grp->nbits, d, f_rng, p_rng);
    }
#endif /* MBEDTLS_ECP_MONTGOMERY_ENABLED */

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS) {
        return mbedtls_ecp_gen_privkey_sw(&grp->N, d, f_rng, p_rng);
    }
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
}

#if defined(MBEDTLS_ECP_C)
/*
 * Generate a keypair with configurable base point
 */
int mbedtls_ecp_gen_keypair_base(mbedtls_ecp_group *grp,
                                 const mbedtls_ecp_point *G,
                                 mbedtls_mpi *d, mbedtls_ecp_point *Q,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_MPI_CHK(mbedtls_ecp_gen_privkey(grp, d, f_rng, p_rng));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(grp, Q, d, G, f_rng, p_rng));

cleanup:
    return ret;
}

/*
 * Generate key pair, wrapper for conventional base point
 */
int mbedtls_ecp_gen_keypair(mbedtls_ecp_group *grp,
                            mbedtls_mpi *d, mbedtls_ecp_point *Q,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    return mbedtls_ecp_gen_keypair_base(grp, &grp->G, d, Q, f_rng, p_rng);
}

/*
 * Generate a keypair, prettier wrapper
 */
int mbedtls_ecp_gen_key(mbedtls_ecp_group_id grp_id, mbedtls_ecp_keypair *key,
                        int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    if ((ret = mbedtls_ecp_group_load(&key->grp, grp_id)) != 0) {
        return ret;
    }

    return mbedtls_ecp_gen_keypair(&key->grp, &key->d, &key->Q, f_rng, p_rng);
}
#endif /* MBEDTLS_ECP_C */

int mbedtls_ecp_set_public_key(mbedtls_ecp_group_id grp_id,
                               mbedtls_ecp_keypair *key,
                               const mbedtls_ecp_point *Q)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (key->grp.id == MBEDTLS_ECP_DP_NONE) {
        /* Group not set yet */
        if ((ret = mbedtls_ecp_group_load(&key->grp, grp_id)) != 0) {
            return ret;
        }
    } else if (key->grp.id != grp_id) {
        /* Group mismatch */
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
    return mbedtls_ecp_copy(&key->Q, Q);
}


#define ECP_CURVE25519_KEY_SIZE 32
#define ECP_CURVE448_KEY_SIZE   56
/*
 * Read a private key.
 */
int mbedtls_ecp_read_key(mbedtls_ecp_group_id grp_id, mbedtls_ecp_keypair *key,
                         const unsigned char *buf, size_t buflen)
{
    int ret = 0;

    if ((ret = mbedtls_ecp_group_load(&key->grp, grp_id)) != 0) {
        return ret;
    }

    ret = MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
    if (mbedtls_ecp_get_type(&key->grp) == MBEDTLS_ECP_TYPE_MONTGOMERY) {
        /*
         * Mask the key as mandated by RFC7748 for Curve25519 and Curve448.
         */
        if (grp_id == MBEDTLS_ECP_DP_CURVE25519) {
            if (buflen != ECP_CURVE25519_KEY_SIZE) {
                return MBEDTLS_ERR_ECP_INVALID_KEY;
            }

            MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary_le(&key->d, buf, buflen));

            /* Set the three least significant bits to 0 */
            MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(&key->d, 0, 0));
            MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(&key->d, 1, 0));
            MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(&key->d, 2, 0));

            /* Set the most significant bit to 0 */
            MBEDTLS_MPI_CHK(
                mbedtls_mpi_set_bit(&key->d,
                                    ECP_CURVE25519_KEY_SIZE * 8 - 1, 0)
                );

            /* Set the second most significant bit to 1 */
            MBEDTLS_MPI_CHK(
                mbedtls_mpi_set_bit(&key->d,
                                    ECP_CURVE25519_KEY_SIZE * 8 - 2, 1)
                );
        } else if (grp_id == MBEDTLS_ECP_DP_CURVE448) {
            if (buflen != ECP_CURVE448_KEY_SIZE) {
                return MBEDTLS_ERR_ECP_INVALID_KEY;
            }

            MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary_le(&key->d, buf, buflen));

            /* Set the two least significant bits to 0 */
            MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(&key->d, 0, 0));
            MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(&key->d, 1, 0));

            /* Set the most significant bit to 1 */
            MBEDTLS_MPI_CHK(
                mbedtls_mpi_set_bit(&key->d,
                                    ECP_CURVE448_KEY_SIZE * 8 - 1, 1)
                );
        }
    }
#endif
#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(&key->grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&key->d, buf, buflen));
    }
#endif

    if (ret == 0) {
        MBEDTLS_MPI_CHK(mbedtls_ecp_check_privkey(&key->grp, &key->d));
    }

cleanup:

    if (ret != 0) {
        mbedtls_mpi_free(&key->d);
    }

    return ret;
}

/*
 * Write a private key.
 */
#if !defined MBEDTLS_DEPRECATED_REMOVED
int mbedtls_ecp_write_key(mbedtls_ecp_keypair *key,
                          unsigned char *buf, size_t buflen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
    if (mbedtls_ecp_get_type(&key->grp) == MBEDTLS_ECP_TYPE_MONTGOMERY) {
        if (key->grp.id == MBEDTLS_ECP_DP_CURVE25519) {
            if (buflen < ECP_CURVE25519_KEY_SIZE) {
                return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
            }

        } else if (key->grp.id == MBEDTLS_ECP_DP_CURVE448) {
            if (buflen < ECP_CURVE448_KEY_SIZE) {
                return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
            }
        }
        MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary_le(&key->d, buf, buflen));
    }
#endif
#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(&key->grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&key->d, buf, buflen));
    }

#endif
cleanup:

    return ret;
}
#endif /* MBEDTLS_DEPRECATED_REMOVED */

int mbedtls_ecp_write_key_ext(const mbedtls_ecp_keypair *key,
                              size_t *olen, unsigned char *buf, size_t buflen)
{
    size_t len = (key->grp.nbits + 7) / 8;
    if (len > buflen) {
        /* For robustness, ensure *olen <= buflen even on error. */
        *olen = 0;
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }
    *olen = len;

    /* Private key not set */
    if (key->d.n == 0) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
    if (mbedtls_ecp_get_type(&key->grp) == MBEDTLS_ECP_TYPE_MONTGOMERY) {
        return mbedtls_mpi_write_binary_le(&key->d, buf, len);
    }
#endif

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    if (mbedtls_ecp_get_type(&key->grp) == MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS) {
        return mbedtls_mpi_write_binary(&key->d, buf, len);
    }
#endif

    /* Private key set but no recognized curve type? This shouldn't happen. */
    return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
}

/*
 * Write a public key.
 */
int mbedtls_ecp_write_public_key(const mbedtls_ecp_keypair *key,
                                 int format, size_t *olen,
                                 unsigned char *buf, size_t buflen)
{
    return mbedtls_ecp_point_write_binary(&key->grp, &key->Q,
                                          format, olen, buf, buflen);
}


#if defined(MBEDTLS_ECP_C)
/*
 * Check a public-private key pair
 */
int mbedtls_ecp_check_pub_priv(
    const mbedtls_ecp_keypair *pub, const mbedtls_ecp_keypair *prv,
    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_point Q;
    mbedtls_ecp_group grp;
    if (pub->grp.id == MBEDTLS_ECP_DP_NONE ||
        pub->grp.id != prv->grp.id ||
        mbedtls_mpi_cmp_mpi(&pub->Q.X, &prv->Q.X) ||
        mbedtls_mpi_cmp_mpi(&pub->Q.Y, &prv->Q.Y) ||
        mbedtls_mpi_cmp_mpi(&pub->Q.Z, &prv->Q.Z)) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    mbedtls_ecp_point_init(&Q);
    mbedtls_ecp_group_init(&grp);

    /* mbedtls_ecp_mul() needs a non-const group... */
    mbedtls_ecp_group_copy(&grp, &prv->grp);

    /* Also checks d is valid */
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&grp, &Q, &prv->d, &prv->grp.G, f_rng, p_rng));

    if (mbedtls_mpi_cmp_mpi(&Q.X, &prv->Q.X) ||
        mbedtls_mpi_cmp_mpi(&Q.Y, &prv->Q.Y) ||
        mbedtls_mpi_cmp_mpi(&Q.Z, &prv->Q.Z)) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_point_free(&Q);
    mbedtls_ecp_group_free(&grp);

    return ret;
}

int mbedtls_ecp_keypair_calc_public(mbedtls_ecp_keypair *key,
                                    int (*f_rng)(void *, unsigned char *, size_t),
                                    void *p_rng)
{
    return mbedtls_ecp_mul(&key->grp, &key->Q, &key->d, &key->grp.G,
                           f_rng, p_rng);
}
#endif /* MBEDTLS_ECP_C */

mbedtls_ecp_group_id mbedtls_ecp_keypair_get_group_id(
    const mbedtls_ecp_keypair *key)
{
    return key->grp.id;
}

/*
 * Export generic key-pair parameters.
 */
int mbedtls_ecp_export(const mbedtls_ecp_keypair *key, mbedtls_ecp_group *grp,
                       mbedtls_mpi *d, mbedtls_ecp_point *Q)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (grp != NULL && (ret = mbedtls_ecp_group_copy(grp, &key->grp)) != 0) {
        return ret;
    }

    if (d != NULL && (ret = mbedtls_mpi_copy(d, &key->d)) != 0) {
        return ret;
    }

    if (Q != NULL && (ret = mbedtls_ecp_copy(Q, &key->Q)) != 0) {
        return ret;
    }

    return 0;
}

#if defined(MBEDTLS_SELF_TEST)

#if defined(MBEDTLS_ECP_C)
/*
 * PRNG for test - !!!INSECURE NEVER USE IN PRODUCTION!!!
 *
 * This is the linear congruential generator from numerical recipes,
 * except we only use the low byte as the output. See
 * https://en.wikipedia.org/wiki/Linear_congruential_generator#Parameters_in_common_use
 */
static int self_test_rng(void *ctx, unsigned char *out, size_t len)
{
    static uint32_t state = 42;

    (void) ctx;

    for (size_t i = 0; i < len; i++) {
        state = state * 1664525u + 1013904223u;
        out[i] = (unsigned char) state;
    }

    return 0;
}

/* Adjust the exponent to be a valid private point for the specified curve.
 * This is sometimes necessary because we use a single set of exponents
 * for all curves but the validity of values depends on the curve. */
static int self_test_adjust_exponent(const mbedtls_ecp_group *grp,
                                     mbedtls_mpi *m)
{
    int ret = 0;
    switch (grp->id) {
    /* If Curve25519 is available, then that's what we use for the
     * Montgomery test, so we don't need the adjustment code. */
#if !defined(MBEDTLS_ECP_DP_CURVE25519_ENABLED)
#if defined(MBEDTLS_ECP_DP_CURVE448_ENABLED)
        case MBEDTLS_ECP_DP_CURVE448:
            /* Move highest bit from 254 to N-1. Setting bit N-1 is
             * necessary to enforce the highest-bit-set constraint. */
            MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(m, 254, 0));
            MBEDTLS_MPI_CHK(mbedtls_mpi_set_bit(m, grp->nbits, 1));
            /* Copy second-highest bit from 253 to N-2. This is not
             * necessary but improves the test variety a bit. */
            MBEDTLS_MPI_CHK(
                mbedtls_mpi_set_bit(m, grp->nbits - 1,
                                    mbedtls_mpi_get_bit(m, 253)));
            break;
#endif
#endif /* ! defined(MBEDTLS_ECP_DP_CURVE25519_ENABLED) */
        default:
            /* Non-Montgomery curves and Curve25519 need no adjustment. */
            (void) grp;
            (void) m;
            goto cleanup;
    }
cleanup:
    return ret;
}

/* Calculate R = m.P for each m in exponents. Check that the number of
 * basic operations doesn't depend on the value of m. */
static int self_test_point(int verbose,
                           mbedtls_ecp_group *grp,
                           mbedtls_ecp_point *R,
                           mbedtls_mpi *m,
                           const mbedtls_ecp_point *P,
                           const char *const *exponents,
                           size_t n_exponents)
{
    int ret = 0;
    size_t i = 0;
    unsigned long add_c_prev, dbl_c_prev, mul_c_prev;
    add_count = 0;
    dbl_count = 0;
    mul_count = 0;

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(m, 16, exponents[0]));
    MBEDTLS_MPI_CHK(self_test_adjust_exponent(grp, m));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(grp, R, m, P, self_test_rng, NULL));

    for (i = 1; i < n_exponents; i++) {
        add_c_prev = add_count;
        dbl_c_prev = dbl_count;
        mul_c_prev = mul_count;
        add_count = 0;
        dbl_count = 0;
        mul_count = 0;

        MBEDTLS_MPI_CHK(mbedtls_mpi_read_string(m, 16, exponents[i]));
        MBEDTLS_MPI_CHK(self_test_adjust_exponent(grp, m));
        MBEDTLS_MPI_CHK(mbedtls_ecp_mul(grp, R, m, P, self_test_rng, NULL));

        if (add_count != add_c_prev ||
            dbl_count != dbl_c_prev ||
            mul_count != mul_c_prev) {
            ret = 1;
            break;
        }
    }

cleanup:
    if (verbose != 0) {
        if (ret != 0) {
            mbedtls_printf("failed (%u)\n", (unsigned int) i);
        } else {
            mbedtls_printf("passed\n");
        }
    }
    return ret;
}
#endif /* MBEDTLS_ECP_C */

/*
 * Checkup routine
 */
int mbedtls_ecp_self_test(int verbose)
{
#if defined(MBEDTLS_ECP_C)
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point R, P;
    mbedtls_mpi m;

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    /* Exponents especially adapted for secp192k1, which has the lowest
     * order n of all supported curves (secp192r1 is in a slightly larger
     * field but the order of its base point is slightly smaller). */
    const char *sw_exponents[] =
    {
        "000000000000000000000000000000000000000000000001", /* one */
        "FFFFFFFFFFFFFFFFFFFFFFFE26F2FC170F69466A74DEFD8C", /* n - 1 */
        "5EA6F389A38B8BC81E767753B15AA5569E1782E30ABE7D25", /* random */
        "400000000000000000000000000000000000000000000000", /* one and zeros */
        "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", /* all ones */
        "555555555555555555555555555555555555555555555555", /* 101010... */
    };
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */
#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
    const char *m_exponents[] =
    {
        /* Valid private values for Curve25519. In a build with Curve448
         * but not Curve25519, they will be adjusted in
         * self_test_adjust_exponent(). */
        "4000000000000000000000000000000000000000000000000000000000000000",
        "5C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C30",
        "5715ECCE24583F7A7023C24164390586842E816D7280A49EF6DF4EAE6B280BF8",
        "41A2B017516F6D254E1F002BCCBADD54BE30F8CEC737A0E912B4963B6BA74460",
        "5555555555555555555555555555555555555555555555555555555555555550",
        "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF8",
    };
#endif /* MBEDTLS_ECP_MONTGOMERY_ENABLED */

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&R);
    mbedtls_ecp_point_init(&P);
    mbedtls_mpi_init(&m);

#if defined(MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED)
    /* Use secp192r1 if available, or any available curve */
#if defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED)
    MBEDTLS_MPI_CHK(mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP192R1));
#else
    MBEDTLS_MPI_CHK(mbedtls_ecp_group_load(&grp, mbedtls_ecp_curve_list()->grp_id));
#endif

    if (verbose != 0) {
        mbedtls_printf("  ECP SW test #1 (constant op_count, base point G): ");
    }
    /* Do a dummy multiplication first to trigger precomputation */
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&m, 2));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&grp, &P, &m, &grp.G, self_test_rng, NULL));
    ret = self_test_point(verbose,
                          &grp, &R, &m, &grp.G,
                          sw_exponents,
                          sizeof(sw_exponents) / sizeof(sw_exponents[0]));
    if (ret != 0) {
        goto cleanup;
    }

    if (verbose != 0) {
        mbedtls_printf("  ECP SW test #2 (constant op_count, other point): ");
    }
    /* We computed P = 2G last time, use it */
    ret = self_test_point(verbose,
                          &grp, &R, &m, &P,
                          sw_exponents,
                          sizeof(sw_exponents) / sizeof(sw_exponents[0]));
    if (ret != 0) {
        goto cleanup;
    }

    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&R);
#endif /* MBEDTLS_ECP_SHORT_WEIERSTRASS_ENABLED */

#if defined(MBEDTLS_ECP_MONTGOMERY_ENABLED)
    if (verbose != 0) {
        mbedtls_printf("  ECP Montgomery test (constant op_count): ");
    }
#if defined(MBEDTLS_ECP_DP_CURVE25519_ENABLED)
    MBEDTLS_MPI_CHK(mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519));
#elif defined(MBEDTLS_ECP_DP_CURVE448_ENABLED)
    MBEDTLS_MPI_CHK(mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE448));
#else
#error "MBEDTLS_ECP_MONTGOMERY_ENABLED is defined, but no curve is supported for self-test"
#endif
    ret = self_test_point(verbose,
                          &grp, &R, &m, &grp.G,
                          m_exponents,
                          sizeof(m_exponents) / sizeof(m_exponents[0]));
    if (ret != 0) {
        goto cleanup;
    }
#endif /* MBEDTLS_ECP_MONTGOMERY_ENABLED */

cleanup:

    if (ret < 0 && verbose != 0) {
        mbedtls_printf("Unexpected error, return code = %08X\n", (unsigned int) ret);
    }

    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&R);
    mbedtls_ecp_point_free(&P);
    mbedtls_mpi_free(&m);

    if (verbose != 0) {
        mbedtls_printf("\n");
    }

    return ret;
#else /* MBEDTLS_ECP_C */
    (void) verbose;
    return 0;
#endif /* MBEDTLS_ECP_C */
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* !MBEDTLS_ECP_ALT */

#endif /* MBEDTLS_ECP_LIGHT */
