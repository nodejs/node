/**
 * \file bignum.h
 *
 * \brief Multi-precision integer library
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_BIGNUM_H
#define MBEDTLS_BIGNUM_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"
#include "mbedtls/platform_util.h"

#include <stddef.h>
#include <stdint.h>

#if defined(MBEDTLS_FS_IO)
#include <stdio.h>
#endif

/** An error occurred while reading from or writing to a file. */
#define MBEDTLS_ERR_MPI_FILE_IO_ERROR                     -0x0002
/** Bad input parameters to function. */
#define MBEDTLS_ERR_MPI_BAD_INPUT_DATA                    -0x0004
/** There is an invalid character in the digit string. */
#define MBEDTLS_ERR_MPI_INVALID_CHARACTER                 -0x0006
/** The buffer is too small to write to. */
#define MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL                  -0x0008
/** The input arguments are negative or result in illegal output. */
#define MBEDTLS_ERR_MPI_NEGATIVE_VALUE                    -0x000A
/** The input argument for division is zero, which is not allowed. */
#define MBEDTLS_ERR_MPI_DIVISION_BY_ZERO                  -0x000C
/** The input arguments are not acceptable. */
#define MBEDTLS_ERR_MPI_NOT_ACCEPTABLE                    -0x000E
/** Memory allocation failed. */
#define MBEDTLS_ERR_MPI_ALLOC_FAILED                      -0x0010

#define MBEDTLS_MPI_CHK(f)       \
    do                           \
    {                            \
        if ((ret = (f)) != 0) \
        goto cleanup;        \
    } while (0)

/*
 * Maximum size MPIs are allowed to grow to in number of limbs.
 */
#define MBEDTLS_MPI_MAX_LIMBS                             10000

#if !defined(MBEDTLS_MPI_WINDOW_SIZE)
/*
 * Maximum window size used for modular exponentiation. Default: 3
 * Minimum value: 1. Maximum value: 6.
 *
 * Result is an array of ( 2 ** MBEDTLS_MPI_WINDOW_SIZE ) MPIs used
 * for the sliding window calculation. (So 8 by default)
 *
 * Reduction in size, reduces speed.
 */
#define MBEDTLS_MPI_WINDOW_SIZE                           3        /**< Maximum window size used. */
#endif /* !MBEDTLS_MPI_WINDOW_SIZE */

#if !defined(MBEDTLS_MPI_MAX_SIZE)
/*
 * Maximum size of MPIs allowed in bits and bytes for user-MPIs.
 * ( Default: 512 bytes => 4096 bits, Maximum tested: 2048 bytes => 16384 bits )
 *
 * Note: Calculations can temporarily result in larger MPIs. So the number
 * of limbs required (MBEDTLS_MPI_MAX_LIMBS) is higher.
 */
#define MBEDTLS_MPI_MAX_SIZE                              1024     /**< Maximum number of bytes for usable MPIs. */
#endif /* !MBEDTLS_MPI_MAX_SIZE */

#define MBEDTLS_MPI_MAX_BITS                              (8 * MBEDTLS_MPI_MAX_SIZE)      /**< Maximum number of bits for usable MPIs. */

/*
 * When reading from files with mbedtls_mpi_read_file() and writing to files with
 * mbedtls_mpi_write_file() the buffer should have space
 * for a (short) label, the MPI (in the provided radix), the newline
 * characters and the '\0'.
 *
 * By default we assume at least a 10 char label, a minimum radix of 10
 * (decimal) and a maximum of 4096 bit numbers (1234 decimal chars).
 * Autosized at compile time for at least a 10 char label, a minimum radix
 * of 10 (decimal) for a number of MBEDTLS_MPI_MAX_BITS size.
 *
 * This used to be statically sized to 1250 for a maximum of 4096 bit
 * numbers (1234 decimal chars).
 *
 * Calculate using the formula:
 *  MBEDTLS_MPI_RW_BUFFER_SIZE = ceil(MBEDTLS_MPI_MAX_BITS / ln(10) * ln(2)) +
 *                                LabelSize + 6
 */
#define MBEDTLS_MPI_MAX_BITS_SCALE100          (100 * MBEDTLS_MPI_MAX_BITS)
#define MBEDTLS_LN_2_DIV_LN_10_SCALE100                 332
#define MBEDTLS_MPI_RW_BUFFER_SIZE             (((MBEDTLS_MPI_MAX_BITS_SCALE100 + \
                                                  MBEDTLS_LN_2_DIV_LN_10_SCALE100 - 1) / \
                                                 MBEDTLS_LN_2_DIV_LN_10_SCALE100) + 10 + 6)

/*
 * Define the base integer type, architecture-wise.
 *
 * 32 or 64-bit integer types can be forced regardless of the underlying
 * architecture by defining MBEDTLS_HAVE_INT32 or MBEDTLS_HAVE_INT64
 * respectively and undefining MBEDTLS_HAVE_ASM.
 *
 * Double-width integers (e.g. 128-bit in 64-bit architectures) can be
 * disabled by defining MBEDTLS_NO_UDBL_DIVISION.
 */
#if !defined(MBEDTLS_HAVE_INT32)
    #if defined(_MSC_VER) && defined(_M_AMD64)
/* Always choose 64-bit when using MSC */
        #if !defined(MBEDTLS_HAVE_INT64)
            #define MBEDTLS_HAVE_INT64
        #endif /* !MBEDTLS_HAVE_INT64 */
typedef  int64_t mbedtls_mpi_sint;
typedef uint64_t mbedtls_mpi_uint;
#define MBEDTLS_MPI_UINT_MAX                UINT64_MAX
    #elif defined(__GNUC__) && (                         \
    defined(__amd64__) || defined(__x86_64__)     || \
    defined(__ppc64__) || defined(__powerpc64__)  || \
    defined(__ia64__)  || defined(__alpha__)      || \
    (defined(__sparc__) && defined(__arch64__)) || \
    defined(__s390x__) || defined(__mips64)       || \
    defined(__aarch64__))
        #if !defined(MBEDTLS_HAVE_INT64)
            #define MBEDTLS_HAVE_INT64
        #endif /* MBEDTLS_HAVE_INT64 */
typedef  int64_t mbedtls_mpi_sint;
typedef uint64_t mbedtls_mpi_uint;
#define MBEDTLS_MPI_UINT_MAX                UINT64_MAX
        #if !defined(MBEDTLS_NO_UDBL_DIVISION)
/* mbedtls_t_udbl defined as 128-bit unsigned int */
typedef unsigned int mbedtls_t_udbl __attribute__((mode(TI)));
            #define MBEDTLS_HAVE_UDBL
        #endif /* !MBEDTLS_NO_UDBL_DIVISION */
    #elif defined(__ARMCC_VERSION) && defined(__aarch64__)
/*
 * __ARMCC_VERSION is defined for both armcc and armclang and
 * __aarch64__ is only defined by armclang when compiling 64-bit code
 */
        #if !defined(MBEDTLS_HAVE_INT64)
            #define MBEDTLS_HAVE_INT64
        #endif /* !MBEDTLS_HAVE_INT64 */
typedef  int64_t mbedtls_mpi_sint;
typedef uint64_t mbedtls_mpi_uint;
#define MBEDTLS_MPI_UINT_MAX                UINT64_MAX
        #if !defined(MBEDTLS_NO_UDBL_DIVISION)
/* mbedtls_t_udbl defined as 128-bit unsigned int */
typedef __uint128_t mbedtls_t_udbl;
            #define MBEDTLS_HAVE_UDBL
        #endif /* !MBEDTLS_NO_UDBL_DIVISION */
    #elif defined(MBEDTLS_HAVE_INT64)
/* Force 64-bit integers with unknown compiler */
typedef  int64_t mbedtls_mpi_sint;
typedef uint64_t mbedtls_mpi_uint;
#define MBEDTLS_MPI_UINT_MAX                UINT64_MAX
    #endif
#endif /* !MBEDTLS_HAVE_INT32 */

#if !defined(MBEDTLS_HAVE_INT64)
/* Default to 32-bit compilation */
    #if !defined(MBEDTLS_HAVE_INT32)
        #define MBEDTLS_HAVE_INT32
    #endif /* !MBEDTLS_HAVE_INT32 */
typedef  int32_t mbedtls_mpi_sint;
typedef uint32_t mbedtls_mpi_uint;
#define MBEDTLS_MPI_UINT_MAX                UINT32_MAX
    #if !defined(MBEDTLS_NO_UDBL_DIVISION)
typedef uint64_t mbedtls_t_udbl;
        #define MBEDTLS_HAVE_UDBL
    #endif /* !MBEDTLS_NO_UDBL_DIVISION */
#endif /* !MBEDTLS_HAVE_INT64 */

/*
 * Sanity check that exactly one of MBEDTLS_HAVE_INT32 or MBEDTLS_HAVE_INT64 is defined,
 * so that code elsewhere doesn't have to check.
 */
#if (!(defined(MBEDTLS_HAVE_INT32) || defined(MBEDTLS_HAVE_INT64))) || \
    (defined(MBEDTLS_HAVE_INT32) && defined(MBEDTLS_HAVE_INT64))
#error "Only 32-bit or 64-bit limbs are supported in bignum"
#endif

/** \typedef mbedtls_mpi_uint
 * \brief The type of machine digits in a bignum, called _limbs_.
 *
 * This is always an unsigned integer type with no padding bits. The size
 * is platform-dependent.
 */

/** \typedef mbedtls_mpi_sint
 * \brief The signed type corresponding to #mbedtls_mpi_uint.
 *
 * This is always an signed integer type with no padding bits. The size
 * is platform-dependent.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          MPI structure
 */
typedef struct mbedtls_mpi {
    /** Pointer to limbs.
     *
     * This may be \c NULL if \c n is 0.
     */
    mbedtls_mpi_uint *MBEDTLS_PRIVATE(p);

    /** Sign: -1 if the mpi is negative, 1 otherwise.
     *
     * The number 0 must be represented with `s = +1`. Although many library
     * functions treat all-limbs-zero as equivalent to a valid representation
     * of 0 regardless of the sign bit, there are exceptions, so bignum
     * functions and external callers must always set \c s to +1 for the
     * number zero.
     *
     * Note that this implies that calloc() or `... = {0}` does not create
     * a valid MPI representation. You must call mbedtls_mpi_init().
     */
    signed short MBEDTLS_PRIVATE(s);

    /** Total number of limbs in \c p.  */
    unsigned short MBEDTLS_PRIVATE(n);
    /* Make sure that MBEDTLS_MPI_MAX_LIMBS fits in n.
     * Use the same limit value on all platforms so that we don't have to
     * think about different behavior on the rare platforms where
     * unsigned short can store values larger than the minimum required by
     * the C language, which is 65535.
     */
#if MBEDTLS_MPI_MAX_LIMBS > 65535
#error "MBEDTLS_MPI_MAX_LIMBS > 65535 is not supported"
#endif
}
mbedtls_mpi;

/**
 * \brief           Initialize an MPI context.
 *
 *                  This makes the MPI ready to be set or freed,
 *                  but does not define a value for the MPI.
 *
 * \param X         The MPI context to initialize. This must not be \c NULL.
 */
void mbedtls_mpi_init(mbedtls_mpi *X);

/**
 * \brief          This function frees the components of an MPI context.
 *
 * \param X        The MPI context to be cleared. This may be \c NULL,
 *                 in which case this function is a no-op. If it is
 *                 not \c NULL, it must point to an initialized MPI.
 */
void mbedtls_mpi_free(mbedtls_mpi *X);

/**
 * \brief          Enlarge an MPI to the specified number of limbs.
 *
 * \note           This function does nothing if the MPI is
 *                 already large enough.
 *
 * \param X        The MPI to grow. It must be initialized.
 * \param nblimbs  The target number of limbs.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed.
 * \return         Another negative error code on other kinds of failure.
 */
int mbedtls_mpi_grow(mbedtls_mpi *X, size_t nblimbs);

/**
 * \brief          This function resizes an MPI downwards, keeping at least the
 *                 specified number of limbs.
 *
 *                 If \c X is smaller than \c nblimbs, it is resized up
 *                 instead.
 *
 * \param X        The MPI to shrink. This must point to an initialized MPI.
 * \param nblimbs  The minimum number of limbs to keep.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
 *                 (this can only happen when resizing up).
 * \return         Another negative error code on other kinds of failure.
 */
int mbedtls_mpi_shrink(mbedtls_mpi *X, size_t nblimbs);

/**
 * \brief          Make a copy of an MPI.
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param Y        The source MPI. This must point to an initialized MPI.
 *
 * \note           The limb-buffer in the destination MPI is enlarged
 *                 if necessary to hold the value in the source MPI.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed.
 * \return         Another negative error code on other kinds of failure.
 */
int mbedtls_mpi_copy(mbedtls_mpi *X, const mbedtls_mpi *Y);

/**
 * \brief          Swap the contents of two MPIs.
 *
 * \param X        The first MPI. It must be initialized.
 * \param Y        The second MPI. It must be initialized.
 */
void mbedtls_mpi_swap(mbedtls_mpi *X, mbedtls_mpi *Y);

/**
 * \brief          Perform a safe conditional copy of MPI which doesn't
 *                 reveal whether the condition was true or not.
 *
 * \param X        The MPI to conditionally assign to. This must point
 *                 to an initialized MPI.
 * \param Y        The MPI to be assigned from. This must point to an
 *                 initialized MPI.
 * \param assign   The condition deciding whether to perform the
 *                 assignment or not. Must be either 0 or 1:
 *                 * \c 1: Perform the assignment `X = Y`.
 *                 * \c 0: Keep the original value of \p X.
 *
 * \note           This function is equivalent to
 *                      `if( assign ) mbedtls_mpi_copy( X, Y );`
 *                 except that it avoids leaking any information about whether
 *                 the assignment was done or not (the above code may leak
 *                 information through branch prediction and/or memory access
 *                 patterns analysis).
 *
 * \warning        If \p assign is neither 0 nor 1, the result of this function
 *                 is indeterminate, and the resulting value in \p X might be
 *                 neither its original value nor the value in \p Y.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed.
 * \return         Another negative error code on other kinds of failure.
 */
int mbedtls_mpi_safe_cond_assign(mbedtls_mpi *X, const mbedtls_mpi *Y, unsigned char assign);

/**
 * \brief          Perform a safe conditional swap which doesn't
 *                 reveal whether the condition was true or not.
 *
 * \param X        The first MPI. This must be initialized.
 * \param Y        The second MPI. This must be initialized.
 * \param swap     The condition deciding whether to perform
 *                 the swap or not. Must be either 0 or 1:
 *                 * \c 1: Swap the values of \p X and \p Y.
 *                 * \c 0: Keep the original values of \p X and \p Y.
 *
 * \note           This function is equivalent to
 *                      if( swap ) mbedtls_mpi_swap( X, Y );
 *                 except that it avoids leaking any information about whether
 *                 the swap was done or not (the above code may leak
 *                 information through branch prediction and/or memory access
 *                 patterns analysis).
 *
 * \warning        If \p swap is neither 0 nor 1, the result of this function
 *                 is indeterminate, and both \p X and \p Y might end up with
 *                 values different to either of the original ones.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed.
 * \return         Another negative error code on other kinds of failure.
 *
 */
int mbedtls_mpi_safe_cond_swap(mbedtls_mpi *X, mbedtls_mpi *Y, unsigned char swap);

/**
 * \brief          Store integer value in MPI.
 *
 * \param X        The MPI to set. This must be initialized.
 * \param z        The value to use.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed.
 * \return         Another negative error code on other kinds of failure.
 */
int mbedtls_mpi_lset(mbedtls_mpi *X, mbedtls_mpi_sint z);

/**
 * \brief          Get a specific bit from an MPI.
 *
 * \param X        The MPI to query. This must be initialized.
 * \param pos      Zero-based index of the bit to query.
 *
 * \return         \c 0 or \c 1 on success, depending on whether bit \c pos
 *                 of \c X is unset or set.
 * \return         A negative error code on failure.
 */
int mbedtls_mpi_get_bit(const mbedtls_mpi *X, size_t pos);

/**
 * \brief          Modify a specific bit in an MPI.
 *
 * \note           This function will grow the target MPI if necessary to set a
 *                 bit to \c 1 in a not yet existing limb. It will not grow if
 *                 the bit should be set to \c 0.
 *
 * \param X        The MPI to modify. This must be initialized.
 * \param pos      Zero-based index of the bit to modify.
 * \param val      The desired value of bit \c pos: \c 0 or \c 1.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed.
 * \return         Another negative error code on other kinds of failure.
 */
int mbedtls_mpi_set_bit(mbedtls_mpi *X, size_t pos, unsigned char val);

/**
 * \brief          Return the number of bits of value \c 0 before the
 *                 least significant bit of value \c 1.
 *
 * \note           This is the same as the zero-based index of
 *                 the least significant bit of value \c 1.
 *
 * \param X        The MPI to query.
 *
 * \return         The number of bits of value \c 0 before the least significant
 *                 bit of value \c 1 in \p X.
 */
size_t mbedtls_mpi_lsb(const mbedtls_mpi *X);

/**
 * \brief          Return the number of bits up to and including the most
 *                 significant bit of value \c 1.
 *
 * * \note         This is same as the one-based index of the most
 *                 significant bit of value \c 1.
 *
 * \param X        The MPI to query. This must point to an initialized MPI.
 *
 * \return         The number of bits up to and including the most
 *                 significant bit of value \c 1.
 */
size_t mbedtls_mpi_bitlen(const mbedtls_mpi *X);

/**
 * \brief          Return the total size of an MPI value in bytes.
 *
 * \param X        The MPI to use. This must point to an initialized MPI.
 *
 * \note           The value returned by this function may be less than
 *                 the number of bytes used to store \p X internally.
 *                 This happens if and only if there are trailing bytes
 *                 of value zero.
 *
 * \return         The least number of bytes capable of storing
 *                 the absolute value of \p X.
 */
size_t mbedtls_mpi_size(const mbedtls_mpi *X);

/**
 * \brief          Import an MPI from an ASCII string.
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param radix    The numeric base of the input string.
 * \param s        Null-terminated string buffer.
 *
 * \return         \c 0 if successful.
 * \return         A negative error code on failure.
 */
int mbedtls_mpi_read_string(mbedtls_mpi *X, int radix, const char *s);

/**
 * \brief          Export an MPI to an ASCII string.
 *
 * \param X        The source MPI. This must point to an initialized MPI.
 * \param radix    The numeric base of the output string.
 * \param buf      The buffer to write the string to. This must be writable
 *                 buffer of length \p buflen Bytes.
 * \param buflen   The available size in Bytes of \p buf.
 * \param olen     The address at which to store the length of the string
 *                 written, including the  final \c NULL byte. This must
 *                 not be \c NULL.
 *
 * \note           You can call this function with `buflen == 0` to obtain the
 *                 minimum required buffer size in `*olen`.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if the target buffer \p buf
 *                 is too small to hold the value of \p X in the desired base.
 *                 In this case, `*olen` is nonetheless updated to contain the
 *                 size of \p buf required for a successful call.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_write_string(const mbedtls_mpi *X, int radix,
                             char *buf, size_t buflen, size_t *olen);

#if defined(MBEDTLS_FS_IO)
/**
 * \brief          Read an MPI from a line in an opened file.
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param radix    The numeric base of the string representation used
 *                 in the source line.
 * \param fin      The input file handle to use. This must not be \c NULL.
 *
 * \note           On success, this function advances the file stream
 *                 to the end of the current line or to EOF.
 *
 *                 The function returns \c 0 on an empty line.
 *
 *                 Leading whitespaces are ignored, as is a
 *                 '0x' prefix for radix \c 16.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if the file read buffer
 *                 is too small.
 * \return         Another negative error code on failure.
 */
int mbedtls_mpi_read_file(mbedtls_mpi *X, int radix, FILE *fin);

/**
 * \brief          Export an MPI into an opened file.
 *
 * \param p        A string prefix to emit prior to the MPI data.
 *                 For example, this might be a label, or "0x" when
 *                 printing in base \c 16. This may be \c NULL if no prefix
 *                 is needed.
 * \param X        The source MPI. This must point to an initialized MPI.
 * \param radix    The numeric base to be used in the emitted string.
 * \param fout     The output file handle. This may be \c NULL, in which case
 *                 the output is written to \c stdout.
 *
 * \return         \c 0 if successful.
 * \return         A negative error code on failure.
 */
int mbedtls_mpi_write_file(const char *p, const mbedtls_mpi *X,
                           int radix, FILE *fout);
#endif /* MBEDTLS_FS_IO */

/**
 * \brief          Import an MPI from unsigned big endian binary data.
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param buf      The input buffer. This must be a readable buffer of length
 *                 \p buflen Bytes.
 * \param buflen   The length of the input buffer \p buf in Bytes.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_read_binary(mbedtls_mpi *X, const unsigned char *buf,
                            size_t buflen);

/**
 * \brief          Import X from unsigned binary data, little endian
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param buf      The input buffer. This must be a readable buffer of length
 *                 \p buflen Bytes.
 * \param buflen   The length of the input buffer \p buf in Bytes.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_read_binary_le(mbedtls_mpi *X,
                               const unsigned char *buf, size_t buflen);

/**
 * \brief          Export X into unsigned binary data, big endian.
 *                 Always fills the whole buffer, which will start with zeros
 *                 if the number is smaller.
 *
 * \param X        The source MPI. This must point to an initialized MPI.
 * \param buf      The output buffer. This must be a writable buffer of length
 *                 \p buflen Bytes.
 * \param buflen   The size of the output buffer \p buf in Bytes.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if \p buf isn't
 *                 large enough to hold the value of \p X.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_write_binary(const mbedtls_mpi *X, unsigned char *buf,
                             size_t buflen);

/**
 * \brief          Export X into unsigned binary data, little endian.
 *                 Always fills the whole buffer, which will end with zeros
 *                 if the number is smaller.
 *
 * \param X        The source MPI. This must point to an initialized MPI.
 * \param buf      The output buffer. This must be a writable buffer of length
 *                 \p buflen Bytes.
 * \param buflen   The size of the output buffer \p buf in Bytes.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL if \p buf isn't
 *                 large enough to hold the value of \p X.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_write_binary_le(const mbedtls_mpi *X,
                                unsigned char *buf, size_t buflen);

/**
 * \brief          Perform a left-shift on an MPI: X <<= count
 *
 * \param X        The MPI to shift. This must point to an initialized MPI.
 *                 The MPI pointed by \p X may be resized to fit
 *                 the resulting number.
 * \param count    The number of bits to shift by.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_shift_l(mbedtls_mpi *X, size_t count);

/**
 * \brief          Perform a right-shift on an MPI: X >>= count
 *
 * \param X        The MPI to shift. This must point to an initialized MPI.
 * \param count    The number of bits to shift by.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_shift_r(mbedtls_mpi *X, size_t count);

/**
 * \brief          Compare the absolute values of two MPIs.
 *
 * \param X        The left-hand MPI. This must point to an initialized MPI.
 * \param Y        The right-hand MPI. This must point to an initialized MPI.
 *
 * \return         \c 1 if `|X|` is greater than `|Y|`.
 * \return         \c -1 if `|X|` is lesser than `|Y|`.
 * \return         \c 0 if `|X|` is equal to `|Y|`.
 */
int mbedtls_mpi_cmp_abs(const mbedtls_mpi *X, const mbedtls_mpi *Y);

/**
 * \brief          Compare two MPIs.
 *
 * \param X        The left-hand MPI. This must point to an initialized MPI.
 * \param Y        The right-hand MPI. This must point to an initialized MPI.
 *
 * \return         \c 1 if \p X is greater than \p Y.
 * \return         \c -1 if \p X is lesser than \p Y.
 * \return         \c 0 if \p X is equal to \p Y.
 */
int mbedtls_mpi_cmp_mpi(const mbedtls_mpi *X, const mbedtls_mpi *Y);

/**
 * \brief          Check if an MPI is less than the other in constant time.
 *
 * \param X        The left-hand MPI. This must point to an initialized MPI
 *                 with the same allocated length as Y.
 * \param Y        The right-hand MPI. This must point to an initialized MPI
 *                 with the same allocated length as X.
 * \param ret      The result of the comparison:
 *                 \c 1 if \p X is less than \p Y.
 *                 \c 0 if \p X is greater than or equal to \p Y.
 *
 * \return         0 on success.
 * \return         MBEDTLS_ERR_MPI_BAD_INPUT_DATA if the allocated length of
 *                 the two input MPIs is not the same.
 */
int mbedtls_mpi_lt_mpi_ct(const mbedtls_mpi *X, const mbedtls_mpi *Y,
                          unsigned *ret);

/**
 * \brief          Compare an MPI with an integer.
 *
 * \param X        The left-hand MPI. This must point to an initialized MPI.
 * \param z        The integer value to compare \p X to.
 *
 * \return         \c 1 if \p X is greater than \p z.
 * \return         \c -1 if \p X is lesser than \p z.
 * \return         \c 0 if \p X is equal to \p z.
 */
int mbedtls_mpi_cmp_int(const mbedtls_mpi *X, mbedtls_mpi_sint z);

/**
 * \brief          Perform an unsigned addition of MPIs: X = |A| + |B|
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param A        The first summand. This must point to an initialized MPI.
 * \param B        The second summand. This must point to an initialized MPI.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_add_abs(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *B);

/**
 * \brief          Perform an unsigned subtraction of MPIs: X = |A| - |B|
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param A        The minuend. This must point to an initialized MPI.
 * \param B        The subtrahend. This must point to an initialized MPI.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_NEGATIVE_VALUE if \p B is greater than \p A.
 * \return         Another negative error code on different kinds of failure.
 *
 */
int mbedtls_mpi_sub_abs(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *B);

/**
 * \brief          Perform a signed addition of MPIs: X = A + B
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param A        The first summand. This must point to an initialized MPI.
 * \param B        The second summand. This must point to an initialized MPI.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_add_mpi(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *B);

/**
 * \brief          Perform a signed subtraction of MPIs: X = A - B
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param A        The minuend. This must point to an initialized MPI.
 * \param B        The subtrahend. This must point to an initialized MPI.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_sub_mpi(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *B);

/**
 * \brief          Perform a signed addition of an MPI and an integer: X = A + b
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param A        The first summand. This must point to an initialized MPI.
 * \param b        The second summand.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_add_int(mbedtls_mpi *X, const mbedtls_mpi *A,
                        mbedtls_mpi_sint b);

/**
 * \brief          Perform a signed subtraction of an MPI and an integer:
 *                 X = A - b
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param A        The minuend. This must point to an initialized MPI.
 * \param b        The subtrahend.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_sub_int(mbedtls_mpi *X, const mbedtls_mpi *A,
                        mbedtls_mpi_sint b);

/**
 * \brief          Perform a multiplication of two MPIs: X = A * B
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param A        The first factor. This must point to an initialized MPI.
 * \param B        The second factor. This must point to an initialized MPI.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 *
 */
int mbedtls_mpi_mul_mpi(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *B);

/**
 * \brief          Perform a multiplication of an MPI with an unsigned integer:
 *                 X = A * b
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param A        The first factor. This must point to an initialized MPI.
 * \param b        The second factor.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 *
 */
int mbedtls_mpi_mul_int(mbedtls_mpi *X, const mbedtls_mpi *A,
                        mbedtls_mpi_uint b);

/**
 * \brief          Perform a division with remainder of two MPIs:
 *                 A = Q * B + R
 *
 * \param Q        The destination MPI for the quotient.
 *                 This may be \c NULL if the value of the
 *                 quotient is not needed. This must not alias A or B.
 * \param R        The destination MPI for the remainder value.
 *                 This may be \c NULL if the value of the
 *                 remainder is not needed. This must not alias A or B.
 * \param A        The dividend. This must point to an initialized MPI.
 * \param B        The divisor. This must point to an initialized MPI.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_DIVISION_BY_ZERO if \p B equals zero.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_div_mpi(mbedtls_mpi *Q, mbedtls_mpi *R, const mbedtls_mpi *A,
                        const mbedtls_mpi *B);

/**
 * \brief          Perform a division with remainder of an MPI by an integer:
 *                 A = Q * b + R
 *
 * \param Q        The destination MPI for the quotient.
 *                 This may be \c NULL if the value of the
 *                 quotient is not needed.  This must not alias A.
 * \param R        The destination MPI for the remainder value.
 *                 This may be \c NULL if the value of the
 *                 remainder is not needed.  This must not alias A.
 * \param A        The dividend. This must point to an initialized MPi.
 * \param b        The divisor.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_DIVISION_BY_ZERO if \p b equals zero.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_div_int(mbedtls_mpi *Q, mbedtls_mpi *R, const mbedtls_mpi *A,
                        mbedtls_mpi_sint b);

/**
 * \brief          Perform a modular reduction. R = A mod B
 *
 * \param R        The destination MPI for the residue value.
 *                 This must point to an initialized MPI.
 * \param A        The MPI to compute the residue of.
 *                 This must point to an initialized MPI.
 * \param B        The base of the modular reduction.
 *                 This must point to an initialized MPI.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_DIVISION_BY_ZERO if \p B equals zero.
 * \return         #MBEDTLS_ERR_MPI_NEGATIVE_VALUE if \p B is negative.
 * \return         Another negative error code on different kinds of failure.
 *
 */
int mbedtls_mpi_mod_mpi(mbedtls_mpi *R, const mbedtls_mpi *A,
                        const mbedtls_mpi *B);

/**
 * \brief          Perform a modular reduction with respect to an integer.
 *                 r = A mod b
 *
 * \param r        The address at which to store the residue.
 *                 This must not be \c NULL.
 * \param A        The MPI to compute the residue of.
 *                 This must point to an initialized MPi.
 * \param b        The integer base of the modular reduction.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_DIVISION_BY_ZERO if \p b equals zero.
 * \return         #MBEDTLS_ERR_MPI_NEGATIVE_VALUE if \p b is negative.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_mod_int(mbedtls_mpi_uint *r, const mbedtls_mpi *A,
                        mbedtls_mpi_sint b);

/**
 * \brief          Perform a modular exponentiation: X = A^E mod N
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 *                 This must not alias E or N.
 * \param A        The base of the exponentiation.
 *                 This must point to an initialized MPI.
 * \param E        The exponent MPI. This must point to an initialized MPI.
 * \param N        The base for the modular reduction. This must point to an
 *                 initialized MPI.
 * \param prec_RR  A helper MPI depending solely on \p N which can be used to
 *                 speed-up multiple modular exponentiations for the same value
 *                 of \p N. This may be \c NULL. If it is not \c NULL, it must
 *                 point to an initialized MPI. If it hasn't been used after
 *                 the call to mbedtls_mpi_init(), this function will compute
 *                 the helper value and store it in \p prec_RR for reuse on
 *                 subsequent calls to this function. Otherwise, the function
 *                 will assume that \p prec_RR holds the helper value set by a
 *                 previous call to mbedtls_mpi_exp_mod(), and reuse it.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if \c N is negative or
 *                 even, or if \c E is negative.
 * \return         Another negative error code on different kinds of failures.
 *
 */
int mbedtls_mpi_exp_mod(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *E, const mbedtls_mpi *N,
                        mbedtls_mpi *prec_RR);

/**
 * \brief          Fill an MPI with a number of random bytes.
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param size     The number of random bytes to generate.
 * \param f_rng    The RNG function to use. This must not be \c NULL.
 * \param p_rng    The RNG parameter to be passed to \p f_rng. This may be
 *                 \c NULL if \p f_rng doesn't need a context argument.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         Another negative error code on failure.
 *
 * \note           The bytes obtained from the RNG are interpreted
 *                 as a big-endian representation of an MPI; this can
 *                 be relevant in applications like deterministic ECDSA.
 */
int mbedtls_mpi_fill_random(mbedtls_mpi *X, size_t size,
                            mbedtls_f_rng_t *f_rng,
                            void *p_rng);

/** Generate a random number uniformly in a range.
 *
 * This function generates a random number between \p min inclusive and
 * \p N exclusive.
 *
 * The procedure complies with RFC 6979 §3.3 (deterministic ECDSA)
 * when the RNG is a suitably parametrized instance of HMAC_DRBG
 * and \p min is \c 1.
 *
 * \note           There are `N - min` possible outputs. The lower bound
 *                 \p min can be reached, but the upper bound \p N cannot.
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param min      The minimum value to return.
 *                 It must be nonnegative.
 * \param N        The upper bound of the range, exclusive.
 *                 In other words, this is one plus the maximum value to return.
 *                 \p N must be strictly larger than \p min.
 * \param f_rng    The RNG function to use. This must not be \c NULL.
 * \param p_rng    The RNG parameter to be passed to \p f_rng.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if \p min or \p N is invalid
 *                 or if they are incompatible.
 * \return         #MBEDTLS_ERR_MPI_NOT_ACCEPTABLE if the implementation was
 *                 unable to find a suitable value within a limited number
 *                 of attempts. This has a negligible probability if \p N
 *                 is significantly larger than \p min, which is the case
 *                 for all usual cryptographic applications.
 * \return         Another negative error code on failure.
 */
int mbedtls_mpi_random(mbedtls_mpi *X,
                       mbedtls_mpi_sint min,
                       const mbedtls_mpi *N,
                       mbedtls_f_rng_t *f_rng,
                       void *p_rng);

/**
 * \brief          Compute the greatest common divisor: G = gcd(A, B)
 *
 * \param G        The destination MPI. This must point to an initialized MPI.
 * \param A        The first operand. This must point to an initialized MPI.
 * \param B        The second operand. This must point to an initialized MPI.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         Another negative error code on different kinds of failure.
 */
int mbedtls_mpi_gcd(mbedtls_mpi *G, const mbedtls_mpi *A,
                    const mbedtls_mpi *B);

/**
 * \brief          Compute the modular inverse: X = A^-1 mod N
 *
 * \param X        The destination MPI. This must point to an initialized MPI.
 * \param A        The MPI to calculate the modular inverse of. This must point
 *                 to an initialized MPI.
 * \param N        The base of the modular inversion. This must point to an
 *                 initialized MPI.
 *
 * \return         \c 0 if successful.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if \p N is less than
 *                 or equal to one.
 * \return         #MBEDTLS_ERR_MPI_NOT_ACCEPTABLE if \p A has no modular
 *                 inverse with respect to \p N.
 */
int mbedtls_mpi_inv_mod(mbedtls_mpi *X, const mbedtls_mpi *A,
                        const mbedtls_mpi *N);

/**
 * \brief          Miller-Rabin primality test.
 *
 * \warning        If \p X is potentially generated by an adversary, for example
 *                 when validating cryptographic parameters that you didn't
 *                 generate yourself and that are supposed to be prime, then
 *                 \p rounds should be at least the half of the security
 *                 strength of the cryptographic algorithm. On the other hand,
 *                 if \p X is chosen uniformly or non-adversarially (as is the
 *                 case when mbedtls_mpi_gen_prime calls this function), then
 *                 \p rounds can be much lower.
 *
 * \param X        The MPI to check for primality.
 *                 This must point to an initialized MPI.
 * \param rounds   The number of bases to perform the Miller-Rabin primality
 *                 test for. The probability of returning 0 on a composite is
 *                 at most 2<sup>-2*\p rounds </sup>.
 * \param f_rng    The RNG function to use. This must not be \c NULL.
 * \param p_rng    The RNG parameter to be passed to \p f_rng.
 *                 This may be \c NULL if \p f_rng doesn't use
 *                 a context parameter.
 *
 * \return         \c 0 if successful, i.e. \p X is probably prime.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_NOT_ACCEPTABLE if \p X is not prime.
 * \return         Another negative error code on other kinds of failure.
 */
int mbedtls_mpi_is_prime_ext(const mbedtls_mpi *X, int rounds,
                             mbedtls_f_rng_t *f_rng,
                             void *p_rng);
/**
 * \brief Flags for mbedtls_mpi_gen_prime()
 *
 * Each of these flags is a constraint on the result X returned by
 * mbedtls_mpi_gen_prime().
 */
typedef enum {
    MBEDTLS_MPI_GEN_PRIME_FLAG_DH =      0x0001, /**< (X-1)/2 is prime too */
    MBEDTLS_MPI_GEN_PRIME_FLAG_LOW_ERR = 0x0002, /**< lower error rate from 2<sup>-80</sup> to 2<sup>-128</sup> */
} mbedtls_mpi_gen_prime_flag_t;

/**
 * \brief          Generate a prime number.
 *
 * \param X        The destination MPI to store the generated prime in.
 *                 This must point to an initialized MPi.
 * \param nbits    The required size of the destination MPI in bits.
 *                 This must be between \c 3 and #MBEDTLS_MPI_MAX_BITS.
 * \param flags    A mask of flags of type #mbedtls_mpi_gen_prime_flag_t.
 * \param f_rng    The RNG function to use. This must not be \c NULL.
 * \param p_rng    The RNG parameter to be passed to \p f_rng.
 *                 This may be \c NULL if \p f_rng doesn't use
 *                 a context parameter.
 *
 * \return         \c 0 if successful, in which case \p X holds a
 *                 probably prime number.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED if a memory allocation failed.
 * \return         #MBEDTLS_ERR_MPI_BAD_INPUT_DATA if `nbits` is not between
 *                 \c 3 and #MBEDTLS_MPI_MAX_BITS.
 */
int mbedtls_mpi_gen_prime(mbedtls_mpi *X, size_t nbits, int flags,
                          mbedtls_f_rng_t *f_rng,
                          void *p_rng);

#if defined(MBEDTLS_SELF_TEST)

/**
 * \brief          Checkup routine
 *
 * \return         0 if successful, or 1 if the test failed
 */
int mbedtls_mpi_self_test(int verbose);

#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* bignum.h */
