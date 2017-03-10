/*
 * Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * The elliptic curve binary polynomial software is originally written by
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems Laboratories.
 *
 */

#ifndef HEADER_EC_H
# define HEADER_EC_H

# include <openssl/opensslconf.h>

# ifndef OPENSSL_NO_EC
# include <openssl/asn1.h>
# include <openssl/symhacks.h>
# if OPENSSL_API_COMPAT < 0x10100000L
#  include <openssl/bn.h>
# endif
# ifdef  __cplusplus
extern "C" {
# endif

# ifndef OPENSSL_ECC_MAX_FIELD_BITS
#  define OPENSSL_ECC_MAX_FIELD_BITS 661
# endif

/** Enum for the point conversion form as defined in X9.62 (ECDSA)
 *  for the encoding of a elliptic curve point (x,y) */
typedef enum {
        /** the point is encoded as z||x, where the octet z specifies
         *  which solution of the quadratic equation y is  */
    POINT_CONVERSION_COMPRESSED = 2,
        /** the point is encoded as z||x||y, where z is the octet 0x04  */
    POINT_CONVERSION_UNCOMPRESSED = 4,
        /** the point is encoded as z||x||y, where the octet z specifies
         *  which solution of the quadratic equation y is  */
    POINT_CONVERSION_HYBRID = 6
} point_conversion_form_t;

typedef struct ec_method_st EC_METHOD;
typedef struct ec_group_st EC_GROUP;
typedef struct ec_point_st EC_POINT;
typedef struct ecpk_parameters_st ECPKPARAMETERS;
typedef struct ec_parameters_st ECPARAMETERS;

/********************************************************************/
/*               EC_METHODs for curves over GF(p)                   */
/********************************************************************/

/** Returns the basic GFp ec methods which provides the basis for the
 *  optimized methods.
 *  \return  EC_METHOD object
 */
const EC_METHOD *EC_GFp_simple_method(void);

/** Returns GFp methods using montgomery multiplication.
 *  \return  EC_METHOD object
 */
const EC_METHOD *EC_GFp_mont_method(void);

/** Returns GFp methods using optimized methods for NIST recommended curves
 *  \return  EC_METHOD object
 */
const EC_METHOD *EC_GFp_nist_method(void);

# ifndef OPENSSL_NO_EC_NISTP_64_GCC_128
/** Returns 64-bit optimized methods for nistp224
 *  \return  EC_METHOD object
 */
const EC_METHOD *EC_GFp_nistp224_method(void);

/** Returns 64-bit optimized methods for nistp256
 *  \return  EC_METHOD object
 */
const EC_METHOD *EC_GFp_nistp256_method(void);

/** Returns 64-bit optimized methods for nistp521
 *  \return  EC_METHOD object
 */
const EC_METHOD *EC_GFp_nistp521_method(void);
# endif

# ifndef OPENSSL_NO_EC2M
/********************************************************************/
/*           EC_METHOD for curves over GF(2^m)                      */
/********************************************************************/

/** Returns the basic GF2m ec method
 *  \return  EC_METHOD object
 */
const EC_METHOD *EC_GF2m_simple_method(void);

# endif

/********************************************************************/
/*                   EC_GROUP functions                             */
/********************************************************************/

/** Creates a new EC_GROUP object
 *  \param   meth  EC_METHOD to use
 *  \return  newly created EC_GROUP object or NULL in case of an error.
 */
EC_GROUP *EC_GROUP_new(const EC_METHOD *meth);

/** Frees a EC_GROUP object
 *  \param  group  EC_GROUP object to be freed.
 */
void EC_GROUP_free(EC_GROUP *group);

/** Clears and frees a EC_GROUP object
 *  \param  group  EC_GROUP object to be cleared and freed.
 */
void EC_GROUP_clear_free(EC_GROUP *group);

/** Copies EC_GROUP objects. Note: both EC_GROUPs must use the same EC_METHOD.
 *  \param  dst  destination EC_GROUP object
 *  \param  src  source EC_GROUP object
 *  \return 1 on success and 0 if an error occurred.
 */
int EC_GROUP_copy(EC_GROUP *dst, const EC_GROUP *src);

/** Creates a new EC_GROUP object and copies the copies the content
 *  form src to the newly created EC_KEY object
 *  \param  src  source EC_GROUP object
 *  \return newly created EC_GROUP object or NULL in case of an error.
 */
EC_GROUP *EC_GROUP_dup(const EC_GROUP *src);

/** Returns the EC_METHOD of the EC_GROUP object.
 *  \param  group  EC_GROUP object
 *  \return EC_METHOD used in this EC_GROUP object.
 */
const EC_METHOD *EC_GROUP_method_of(const EC_GROUP *group);

/** Returns the field type of the EC_METHOD.
 *  \param  meth  EC_METHOD object
 *  \return NID of the underlying field type OID.
 */
int EC_METHOD_get_field_type(const EC_METHOD *meth);

/** Sets the generator and it's order/cofactor of a EC_GROUP object.
 *  \param  group      EC_GROUP object
 *  \param  generator  EC_POINT object with the generator.
 *  \param  order      the order of the group generated by the generator.
 *  \param  cofactor   the index of the sub-group generated by the generator
 *                     in the group of all points on the elliptic curve.
 *  \return 1 on success and 0 if an error occurred
 */
int EC_GROUP_set_generator(EC_GROUP *group, const EC_POINT *generator,
                           const BIGNUM *order, const BIGNUM *cofactor);

/** Returns the generator of a EC_GROUP object.
 *  \param  group  EC_GROUP object
 *  \return the currently used generator (possibly NULL).
 */
const EC_POINT *EC_GROUP_get0_generator(const EC_GROUP *group);

/** Returns the montgomery data for order(Generator)
 *  \param  group  EC_GROUP object
 *  \return the currently used montgomery data (possibly NULL).
*/
BN_MONT_CTX *EC_GROUP_get_mont_data(const EC_GROUP *group);

/** Gets the order of a EC_GROUP
 *  \param  group  EC_GROUP object
 *  \param  order  BIGNUM to which the order is copied
 *  \param  ctx    unused
 *  \return 1 on success and 0 if an error occurred
 */
int EC_GROUP_get_order(const EC_GROUP *group, BIGNUM *order, BN_CTX *ctx);

/** Gets the order of an EC_GROUP
 *  \param  group  EC_GROUP object
 *  \return the group order
 */
const BIGNUM *EC_GROUP_get0_order(const EC_GROUP *group);

/** Gets the number of bits of the order of an EC_GROUP
 *  \param  group  EC_GROUP object
 *  \return number of bits of group order.
 */
int EC_GROUP_order_bits(const EC_GROUP *group);

/** Gets the cofactor of a EC_GROUP
 *  \param  group     EC_GROUP object
 *  \param  cofactor  BIGNUM to which the cofactor is copied
 *  \param  ctx       unused
 *  \return 1 on success and 0 if an error occurred
 */
int EC_GROUP_get_cofactor(const EC_GROUP *group, BIGNUM *cofactor,
                          BN_CTX *ctx);

/** Gets the cofactor of an EC_GROUP
 *  \param  group  EC_GROUP object
 *  \return the group cofactor
 */
const BIGNUM *EC_GROUP_get0_cofactor(const EC_GROUP *group);

/** Sets the name of a EC_GROUP object
 *  \param  group  EC_GROUP object
 *  \param  nid    NID of the curve name OID
 */
void EC_GROUP_set_curve_name(EC_GROUP *group, int nid);

/** Returns the curve name of a EC_GROUP object
 *  \param  group  EC_GROUP object
 *  \return NID of the curve name OID or 0 if not set.
 */
int EC_GROUP_get_curve_name(const EC_GROUP *group);

void EC_GROUP_set_asn1_flag(EC_GROUP *group, int flag);
int EC_GROUP_get_asn1_flag(const EC_GROUP *group);

void EC_GROUP_set_point_conversion_form(EC_GROUP *group,
                                        point_conversion_form_t form);
point_conversion_form_t EC_GROUP_get_point_conversion_form(const EC_GROUP *);

unsigned char *EC_GROUP_get0_seed(const EC_GROUP *x);
size_t EC_GROUP_get_seed_len(const EC_GROUP *);
size_t EC_GROUP_set_seed(EC_GROUP *, const unsigned char *, size_t len);

/** Sets the parameter of a ec over GFp defined by y^2 = x^3 + a*x + b
 *  \param  group  EC_GROUP object
 *  \param  p      BIGNUM with the prime number
 *  \param  a      BIGNUM with parameter a of the equation
 *  \param  b      BIGNUM with parameter b of the equation
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_GROUP_set_curve_GFp(EC_GROUP *group, const BIGNUM *p, const BIGNUM *a,
                           const BIGNUM *b, BN_CTX *ctx);

/** Gets the parameter of the ec over GFp defined by y^2 = x^3 + a*x + b
 *  \param  group  EC_GROUP object
 *  \param  p      BIGNUM for the prime number
 *  \param  a      BIGNUM for parameter a of the equation
 *  \param  b      BIGNUM for parameter b of the equation
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_GROUP_get_curve_GFp(const EC_GROUP *group, BIGNUM *p, BIGNUM *a,
                           BIGNUM *b, BN_CTX *ctx);

# ifndef OPENSSL_NO_EC2M
/** Sets the parameter of a ec over GF2m defined by y^2 + x*y = x^3 + a*x^2 + b
 *  \param  group  EC_GROUP object
 *  \param  p      BIGNUM with the polynomial defining the underlying field
 *  \param  a      BIGNUM with parameter a of the equation
 *  \param  b      BIGNUM with parameter b of the equation
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_GROUP_set_curve_GF2m(EC_GROUP *group, const BIGNUM *p, const BIGNUM *a,
                            const BIGNUM *b, BN_CTX *ctx);

/** Gets the parameter of the ec over GF2m defined by y^2 + x*y = x^3 + a*x^2 + b
 *  \param  group  EC_GROUP object
 *  \param  p      BIGNUM for the polynomial defining the underlying field
 *  \param  a      BIGNUM for parameter a of the equation
 *  \param  b      BIGNUM for parameter b of the equation
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_GROUP_get_curve_GF2m(const EC_GROUP *group, BIGNUM *p, BIGNUM *a,
                            BIGNUM *b, BN_CTX *ctx);
# endif
/** Returns the number of bits needed to represent a field element
 *  \param  group  EC_GROUP object
 *  \return number of bits needed to represent a field element
 */
int EC_GROUP_get_degree(const EC_GROUP *group);

/** Checks whether the parameter in the EC_GROUP define a valid ec group
 *  \param  group  EC_GROUP object
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 if group is a valid ec group and 0 otherwise
 */
int EC_GROUP_check(const EC_GROUP *group, BN_CTX *ctx);

/** Checks whether the discriminant of the elliptic curve is zero or not
 *  \param  group  EC_GROUP object
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 if the discriminant is not zero and 0 otherwise
 */
int EC_GROUP_check_discriminant(const EC_GROUP *group, BN_CTX *ctx);

/** Compares two EC_GROUP objects
 *  \param  a    first EC_GROUP object
 *  \param  b    second EC_GROUP object
 *  \param  ctx  BN_CTX object (optional)
 *  \return 0 if the groups are equal, 1 if not, or -1 on error
 */
int EC_GROUP_cmp(const EC_GROUP *a, const EC_GROUP *b, BN_CTX *ctx);

/*
 * EC_GROUP_new_GF*() calls EC_GROUP_new() and EC_GROUP_set_GF*() after
 * choosing an appropriate EC_METHOD
 */

/** Creates a new EC_GROUP object with the specified parameters defined
 *  over GFp (defined by the equation y^2 = x^3 + a*x + b)
 *  \param  p    BIGNUM with the prime number
 *  \param  a    BIGNUM with the parameter a of the equation
 *  \param  b    BIGNUM with the parameter b of the equation
 *  \param  ctx  BN_CTX object (optional)
 *  \return newly created EC_GROUP object with the specified parameters
 */
EC_GROUP *EC_GROUP_new_curve_GFp(const BIGNUM *p, const BIGNUM *a,
                                 const BIGNUM *b, BN_CTX *ctx);
# ifndef OPENSSL_NO_EC2M
/** Creates a new EC_GROUP object with the specified parameters defined
 *  over GF2m (defined by the equation y^2 + x*y = x^3 + a*x^2 + b)
 *  \param  p    BIGNUM with the polynomial defining the underlying field
 *  \param  a    BIGNUM with the parameter a of the equation
 *  \param  b    BIGNUM with the parameter b of the equation
 *  \param  ctx  BN_CTX object (optional)
 *  \return newly created EC_GROUP object with the specified parameters
 */
EC_GROUP *EC_GROUP_new_curve_GF2m(const BIGNUM *p, const BIGNUM *a,
                                  const BIGNUM *b, BN_CTX *ctx);
# endif

/** Creates a EC_GROUP object with a curve specified by a NID
 *  \param  nid  NID of the OID of the curve name
 *  \return newly created EC_GROUP object with specified curve or NULL
 *          if an error occurred
 */
EC_GROUP *EC_GROUP_new_by_curve_name(int nid);

/** Creates a new EC_GROUP object from an ECPARAMETERS object
 *  \param  params  pointer to the ECPARAMETERS object
 *  \return newly created EC_GROUP object with specified curve or NULL
 *          if an error occurred
 */
EC_GROUP *EC_GROUP_new_from_ecparameters(const ECPARAMETERS *params);

/** Creates an ECPARAMETERS object for the the given EC_GROUP object.
 *  \param  group   pointer to the EC_GROUP object
 *  \param  params  pointer to an existing ECPARAMETERS object or NULL
 *  \return pointer to the new ECPARAMETERS object or NULL
 *          if an error occurred.
 */
ECPARAMETERS *EC_GROUP_get_ecparameters(const EC_GROUP *group,
                                        ECPARAMETERS *params);

/** Creates a new EC_GROUP object from an ECPKPARAMETERS object
 *  \param  params  pointer to an existing ECPKPARAMETERS object, or NULL
 *  \return newly created EC_GROUP object with specified curve, or NULL
 *          if an error occurred
 */
EC_GROUP *EC_GROUP_new_from_ecpkparameters(const ECPKPARAMETERS *params);

/** Creates an ECPKPARAMETERS object for the the given EC_GROUP object.
 *  \param  group   pointer to the EC_GROUP object
 *  \param  params  pointer to an existing ECPKPARAMETERS object or NULL
 *  \return pointer to the new ECPKPARAMETERS object or NULL
 *          if an error occurred.
 */
ECPKPARAMETERS *EC_GROUP_get_ecpkparameters(const EC_GROUP *group,
                                            ECPKPARAMETERS *params);

/********************************************************************/
/*               handling of internal curves                        */
/********************************************************************/

typedef struct {
    int nid;
    const char *comment;
} EC_builtin_curve;

/*
 * EC_builtin_curves(EC_builtin_curve *r, size_t size) returns number of all
 * available curves or zero if a error occurred. In case r is not zero,
 * nitems EC_builtin_curve structures are filled with the data of the first
 * nitems internal groups
 */
size_t EC_get_builtin_curves(EC_builtin_curve *r, size_t nitems);

const char *EC_curve_nid2nist(int nid);
int EC_curve_nist2nid(const char *name);

/********************************************************************/
/*                    EC_POINT functions                            */
/********************************************************************/

/** Creates a new EC_POINT object for the specified EC_GROUP
 *  \param  group  EC_GROUP the underlying EC_GROUP object
 *  \return newly created EC_POINT object or NULL if an error occurred
 */
EC_POINT *EC_POINT_new(const EC_GROUP *group);

/** Frees a EC_POINT object
 *  \param  point  EC_POINT object to be freed
 */
void EC_POINT_free(EC_POINT *point);

/** Clears and frees a EC_POINT object
 *  \param  point  EC_POINT object to be cleared and freed
 */
void EC_POINT_clear_free(EC_POINT *point);

/** Copies EC_POINT object
 *  \param  dst  destination EC_POINT object
 *  \param  src  source EC_POINT object
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_copy(EC_POINT *dst, const EC_POINT *src);

/** Creates a new EC_POINT object and copies the content of the supplied
 *  EC_POINT
 *  \param  src    source EC_POINT object
 *  \param  group  underlying the EC_GROUP object
 *  \return newly created EC_POINT object or NULL if an error occurred
 */
EC_POINT *EC_POINT_dup(const EC_POINT *src, const EC_GROUP *group);

/** Returns the EC_METHOD used in EC_POINT object
 *  \param  point  EC_POINT object
 *  \return the EC_METHOD used
 */
const EC_METHOD *EC_POINT_method_of(const EC_POINT *point);

/** Sets a point to infinity (neutral element)
 *  \param  group  underlying EC_GROUP object
 *  \param  point  EC_POINT to set to infinity
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_set_to_infinity(const EC_GROUP *group, EC_POINT *point);

/** Sets the jacobian projective coordinates of a EC_POINT over GFp
 *  \param  group  underlying EC_GROUP object
 *  \param  p      EC_POINT object
 *  \param  x      BIGNUM with the x-coordinate
 *  \param  y      BIGNUM with the y-coordinate
 *  \param  z      BIGNUM with the z-coordinate
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_set_Jprojective_coordinates_GFp(const EC_GROUP *group,
                                             EC_POINT *p, const BIGNUM *x,
                                             const BIGNUM *y, const BIGNUM *z,
                                             BN_CTX *ctx);

/** Gets the jacobian projective coordinates of a EC_POINT over GFp
 *  \param  group  underlying EC_GROUP object
 *  \param  p      EC_POINT object
 *  \param  x      BIGNUM for the x-coordinate
 *  \param  y      BIGNUM for the y-coordinate
 *  \param  z      BIGNUM for the z-coordinate
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_get_Jprojective_coordinates_GFp(const EC_GROUP *group,
                                             const EC_POINT *p, BIGNUM *x,
                                             BIGNUM *y, BIGNUM *z,
                                             BN_CTX *ctx);

/** Sets the affine coordinates of a EC_POINT over GFp
 *  \param  group  underlying EC_GROUP object
 *  \param  p      EC_POINT object
 *  \param  x      BIGNUM with the x-coordinate
 *  \param  y      BIGNUM with the y-coordinate
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_set_affine_coordinates_GFp(const EC_GROUP *group, EC_POINT *p,
                                        const BIGNUM *x, const BIGNUM *y,
                                        BN_CTX *ctx);

/** Gets the affine coordinates of a EC_POINT over GFp
 *  \param  group  underlying EC_GROUP object
 *  \param  p      EC_POINT object
 *  \param  x      BIGNUM for the x-coordinate
 *  \param  y      BIGNUM for the y-coordinate
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_get_affine_coordinates_GFp(const EC_GROUP *group,
                                        const EC_POINT *p, BIGNUM *x,
                                        BIGNUM *y, BN_CTX *ctx);

/** Sets the x9.62 compressed coordinates of a EC_POINT over GFp
 *  \param  group  underlying EC_GROUP object
 *  \param  p      EC_POINT object
 *  \param  x      BIGNUM with x-coordinate
 *  \param  y_bit  integer with the y-Bit (either 0 or 1)
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_set_compressed_coordinates_GFp(const EC_GROUP *group,
                                            EC_POINT *p, const BIGNUM *x,
                                            int y_bit, BN_CTX *ctx);
# ifndef OPENSSL_NO_EC2M
/** Sets the affine coordinates of a EC_POINT over GF2m
 *  \param  group  underlying EC_GROUP object
 *  \param  p      EC_POINT object
 *  \param  x      BIGNUM with the x-coordinate
 *  \param  y      BIGNUM with the y-coordinate
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_set_affine_coordinates_GF2m(const EC_GROUP *group, EC_POINT *p,
                                         const BIGNUM *x, const BIGNUM *y,
                                         BN_CTX *ctx);

/** Gets the affine coordinates of a EC_POINT over GF2m
 *  \param  group  underlying EC_GROUP object
 *  \param  p      EC_POINT object
 *  \param  x      BIGNUM for the x-coordinate
 *  \param  y      BIGNUM for the y-coordinate
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_get_affine_coordinates_GF2m(const EC_GROUP *group,
                                         const EC_POINT *p, BIGNUM *x,
                                         BIGNUM *y, BN_CTX *ctx);

/** Sets the x9.62 compressed coordinates of a EC_POINT over GF2m
 *  \param  group  underlying EC_GROUP object
 *  \param  p      EC_POINT object
 *  \param  x      BIGNUM with x-coordinate
 *  \param  y_bit  integer with the y-Bit (either 0 or 1)
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_set_compressed_coordinates_GF2m(const EC_GROUP *group,
                                             EC_POINT *p, const BIGNUM *x,
                                             int y_bit, BN_CTX *ctx);
# endif
/** Encodes a EC_POINT object to a octet string
 *  \param  group  underlying EC_GROUP object
 *  \param  p      EC_POINT object
 *  \param  form   point conversion form
 *  \param  buf    memory buffer for the result. If NULL the function returns
 *                 required buffer size.
 *  \param  len    length of the memory buffer
 *  \param  ctx    BN_CTX object (optional)
 *  \return the length of the encoded octet string or 0 if an error occurred
 */
size_t EC_POINT_point2oct(const EC_GROUP *group, const EC_POINT *p,
                          point_conversion_form_t form,
                          unsigned char *buf, size_t len, BN_CTX *ctx);

/** Decodes a EC_POINT from a octet string
 *  \param  group  underlying EC_GROUP object
 *  \param  p      EC_POINT object
 *  \param  buf    memory buffer with the encoded ec point
 *  \param  len    length of the encoded ec point
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_oct2point(const EC_GROUP *group, EC_POINT *p,
                       const unsigned char *buf, size_t len, BN_CTX *ctx);

/** Encodes an EC_POINT object to an allocated octet string
 *  \param  group  underlying EC_GROUP object
 *  \param  point  EC_POINT object
 *  \param  form   point conversion form
 *  \param  pbuf   returns pointer to allocated buffer
 *  \param  len    length of the memory buffer
 *  \param  ctx    BN_CTX object (optional)
 *  \return the length of the encoded octet string or 0 if an error occurred
 */

size_t EC_POINT_point2buf(const EC_GROUP *group, const EC_POINT *point,
                          point_conversion_form_t form,
                          unsigned char **pbuf, BN_CTX *ctx);

/* other interfaces to point2oct/oct2point: */
BIGNUM *EC_POINT_point2bn(const EC_GROUP *, const EC_POINT *,
                          point_conversion_form_t form, BIGNUM *, BN_CTX *);
EC_POINT *EC_POINT_bn2point(const EC_GROUP *, const BIGNUM *,
                            EC_POINT *, BN_CTX *);
char *EC_POINT_point2hex(const EC_GROUP *, const EC_POINT *,
                         point_conversion_form_t form, BN_CTX *);
EC_POINT *EC_POINT_hex2point(const EC_GROUP *, const char *,
                             EC_POINT *, BN_CTX *);

/********************************************************************/
/*         functions for doing EC_POINT arithmetic                  */
/********************************************************************/

/** Computes the sum of two EC_POINT
 *  \param  group  underlying EC_GROUP object
 *  \param  r      EC_POINT object for the result (r = a + b)
 *  \param  a      EC_POINT object with the first summand
 *  \param  b      EC_POINT object with the second summand
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_add(const EC_GROUP *group, EC_POINT *r, const EC_POINT *a,
                 const EC_POINT *b, BN_CTX *ctx);

/** Computes the double of a EC_POINT
 *  \param  group  underlying EC_GROUP object
 *  \param  r      EC_POINT object for the result (r = 2 * a)
 *  \param  a      EC_POINT object
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_dbl(const EC_GROUP *group, EC_POINT *r, const EC_POINT *a,
                 BN_CTX *ctx);

/** Computes the inverse of a EC_POINT
 *  \param  group  underlying EC_GROUP object
 *  \param  a      EC_POINT object to be inverted (it's used for the result as well)
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_invert(const EC_GROUP *group, EC_POINT *a, BN_CTX *ctx);

/** Checks whether the point is the neutral element of the group
 *  \param  group  the underlying EC_GROUP object
 *  \param  p      EC_POINT object
 *  \return 1 if the point is the neutral element and 0 otherwise
 */
int EC_POINT_is_at_infinity(const EC_GROUP *group, const EC_POINT *p);

/** Checks whether the point is on the curve
 *  \param  group  underlying EC_GROUP object
 *  \param  point  EC_POINT object to check
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 if the point is on the curve, 0 if not, or -1 on error
 */
int EC_POINT_is_on_curve(const EC_GROUP *group, const EC_POINT *point,
                         BN_CTX *ctx);

/** Compares two EC_POINTs
 *  \param  group  underlying EC_GROUP object
 *  \param  a      first EC_POINT object
 *  \param  b      second EC_POINT object
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 if the points are not equal, 0 if they are, or -1 on error
 */
int EC_POINT_cmp(const EC_GROUP *group, const EC_POINT *a, const EC_POINT *b,
                 BN_CTX *ctx);

int EC_POINT_make_affine(const EC_GROUP *group, EC_POINT *point, BN_CTX *ctx);
int EC_POINTs_make_affine(const EC_GROUP *group, size_t num,
                          EC_POINT *points[], BN_CTX *ctx);

/** Computes r = generator * n + sum_{i=0}^{num-1} p[i] * m[i]
 *  \param  group  underlying EC_GROUP object
 *  \param  r      EC_POINT object for the result
 *  \param  n      BIGNUM with the multiplier for the group generator (optional)
 *  \param  num    number further summands
 *  \param  p      array of size num of EC_POINT objects
 *  \param  m      array of size num of BIGNUM objects
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINTs_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *n,
                  size_t num, const EC_POINT *p[], const BIGNUM *m[],
                  BN_CTX *ctx);

/** Computes r = generator * n + q * m
 *  \param  group  underlying EC_GROUP object
 *  \param  r      EC_POINT object for the result
 *  \param  n      BIGNUM with the multiplier for the group generator (optional)
 *  \param  q      EC_POINT object with the first factor of the second summand
 *  \param  m      BIGNUM with the second factor of the second summand
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_POINT_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *n,
                 const EC_POINT *q, const BIGNUM *m, BN_CTX *ctx);

/** Stores multiples of generator for faster point multiplication
 *  \param  group  EC_GROUP object
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */
int EC_GROUP_precompute_mult(EC_GROUP *group, BN_CTX *ctx);

/** Reports whether a precomputation has been done
 *  \param  group  EC_GROUP object
 *  \return 1 if a pre-computation has been done and 0 otherwise
 */
int EC_GROUP_have_precompute_mult(const EC_GROUP *group);

/********************************************************************/
/*                       ASN1 stuff                                 */
/********************************************************************/

DECLARE_ASN1_ITEM(ECPKPARAMETERS)
DECLARE_ASN1_ALLOC_FUNCTIONS(ECPKPARAMETERS)
DECLARE_ASN1_ITEM(ECPARAMETERS)
DECLARE_ASN1_ALLOC_FUNCTIONS(ECPARAMETERS)

/*
 * EC_GROUP_get_basis_type() returns the NID of the basis type used to
 * represent the field elements
 */
int EC_GROUP_get_basis_type(const EC_GROUP *);
# ifndef OPENSSL_NO_EC2M
int EC_GROUP_get_trinomial_basis(const EC_GROUP *, unsigned int *k);
int EC_GROUP_get_pentanomial_basis(const EC_GROUP *, unsigned int *k1,
                                   unsigned int *k2, unsigned int *k3);
# endif

# define OPENSSL_EC_EXPLICIT_CURVE  0x000
# define OPENSSL_EC_NAMED_CURVE     0x001

EC_GROUP *d2i_ECPKParameters(EC_GROUP **, const unsigned char **in, long len);
int i2d_ECPKParameters(const EC_GROUP *, unsigned char **out);

# define d2i_ECPKParameters_bio(bp,x) ASN1_d2i_bio_of(EC_GROUP,NULL,d2i_ECPKParameters,bp,x)
# define i2d_ECPKParameters_bio(bp,x) ASN1_i2d_bio_of_const(EC_GROUP,i2d_ECPKParameters,bp,x)
# define d2i_ECPKParameters_fp(fp,x) (EC_GROUP *)ASN1_d2i_fp(NULL, \
                (char *(*)())d2i_ECPKParameters,(fp),(unsigned char **)(x))
# define i2d_ECPKParameters_fp(fp,x) ASN1_i2d_fp(i2d_ECPKParameters,(fp), \
                (unsigned char *)(x))

int ECPKParameters_print(BIO *bp, const EC_GROUP *x, int off);
# ifndef OPENSSL_NO_STDIO
int ECPKParameters_print_fp(FILE *fp, const EC_GROUP *x, int off);
# endif

/********************************************************************/
/*                      EC_KEY functions                            */
/********************************************************************/

/* some values for the encoding_flag */
# define EC_PKEY_NO_PARAMETERS   0x001
# define EC_PKEY_NO_PUBKEY       0x002

/* some values for the flags field */
# define EC_FLAG_NON_FIPS_ALLOW  0x1
# define EC_FLAG_FIPS_CHECKED    0x2
# define EC_FLAG_COFACTOR_ECDH   0x1000

/** Creates a new EC_KEY object.
 *  \return EC_KEY object or NULL if an error occurred.
 */
EC_KEY *EC_KEY_new(void);

int EC_KEY_get_flags(const EC_KEY *key);

void EC_KEY_set_flags(EC_KEY *key, int flags);

void EC_KEY_clear_flags(EC_KEY *key, int flags);

/** Creates a new EC_KEY object using a named curve as underlying
 *  EC_GROUP object.
 *  \param  nid  NID of the named curve.
 *  \return EC_KEY object or NULL if an error occurred.
 */
EC_KEY *EC_KEY_new_by_curve_name(int nid);

/** Frees a EC_KEY object.
 *  \param  key  EC_KEY object to be freed.
 */
void EC_KEY_free(EC_KEY *key);

/** Copies a EC_KEY object.
 *  \param  dst  destination EC_KEY object
 *  \param  src  src EC_KEY object
 *  \return dst or NULL if an error occurred.
 */
EC_KEY *EC_KEY_copy(EC_KEY *dst, const EC_KEY *src);

/** Creates a new EC_KEY object and copies the content from src to it.
 *  \param  src  the source EC_KEY object
 *  \return newly created EC_KEY object or NULL if an error occurred.
 */
EC_KEY *EC_KEY_dup(const EC_KEY *src);

/** Increases the internal reference count of a EC_KEY object.
 *  \param  key  EC_KEY object
 *  \return 1 on success and 0 if an error occurred.
 */
int EC_KEY_up_ref(EC_KEY *key);

/** Returns the EC_GROUP object of a EC_KEY object
 *  \param  key  EC_KEY object
 *  \return the EC_GROUP object (possibly NULL).
 */
const EC_GROUP *EC_KEY_get0_group(const EC_KEY *key);

/** Sets the EC_GROUP of a EC_KEY object.
 *  \param  key    EC_KEY object
 *  \param  group  EC_GROUP to use in the EC_KEY object (note: the EC_KEY
 *                 object will use an own copy of the EC_GROUP).
 *  \return 1 on success and 0 if an error occurred.
 */
int EC_KEY_set_group(EC_KEY *key, const EC_GROUP *group);

/** Returns the private key of a EC_KEY object.
 *  \param  key  EC_KEY object
 *  \return a BIGNUM with the private key (possibly NULL).
 */
const BIGNUM *EC_KEY_get0_private_key(const EC_KEY *key);

/** Sets the private key of a EC_KEY object.
 *  \param  key  EC_KEY object
 *  \param  prv  BIGNUM with the private key (note: the EC_KEY object
 *               will use an own copy of the BIGNUM).
 *  \return 1 on success and 0 if an error occurred.
 */
int EC_KEY_set_private_key(EC_KEY *key, const BIGNUM *prv);

/** Returns the public key of a EC_KEY object.
 *  \param  key  the EC_KEY object
 *  \return a EC_POINT object with the public key (possibly NULL)
 */
const EC_POINT *EC_KEY_get0_public_key(const EC_KEY *key);

/** Sets the public key of a EC_KEY object.
 *  \param  key  EC_KEY object
 *  \param  pub  EC_POINT object with the public key (note: the EC_KEY object
 *               will use an own copy of the EC_POINT object).
 *  \return 1 on success and 0 if an error occurred.
 */
int EC_KEY_set_public_key(EC_KEY *key, const EC_POINT *pub);

unsigned EC_KEY_get_enc_flags(const EC_KEY *key);
void EC_KEY_set_enc_flags(EC_KEY *eckey, unsigned int flags);
point_conversion_form_t EC_KEY_get_conv_form(const EC_KEY *key);
void EC_KEY_set_conv_form(EC_KEY *eckey, point_conversion_form_t cform);

#define EC_KEY_get_ex_new_index(l, p, newf, dupf, freef) \
    CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_EC_KEY, l, p, newf, dupf, freef)
int EC_KEY_set_ex_data(EC_KEY *key, int idx, void *arg);
void *EC_KEY_get_ex_data(const EC_KEY *key, int idx);

/* wrapper functions for the underlying EC_GROUP object */
void EC_KEY_set_asn1_flag(EC_KEY *eckey, int asn1_flag);

/** Creates a table of pre-computed multiples of the generator to
 *  accelerate further EC_KEY operations.
 *  \param  key  EC_KEY object
 *  \param  ctx  BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred.
 */
int EC_KEY_precompute_mult(EC_KEY *key, BN_CTX *ctx);

/** Creates a new ec private (and optional a new public) key.
 *  \param  key  EC_KEY object
 *  \return 1 on success and 0 if an error occurred.
 */
int EC_KEY_generate_key(EC_KEY *key);

/** Verifies that a private and/or public key is valid.
 *  \param  key  the EC_KEY object
 *  \return 1 on success and 0 otherwise.
 */
int EC_KEY_check_key(const EC_KEY *key);

/** Indicates if an EC_KEY can be used for signing.
 *  \param  key  the EC_KEY object
 *  \return 1 if can can sign and 0 otherwise.
 */
int EC_KEY_can_sign(const EC_KEY *eckey);

/** Sets a public key from affine coordinates performing
 *  necessary NIST PKV tests.
 *  \param  key  the EC_KEY object
 *  \param  x    public key x coordinate
 *  \param  y    public key y coordinate
 *  \return 1 on success and 0 otherwise.
 */
int EC_KEY_set_public_key_affine_coordinates(EC_KEY *key, BIGNUM *x,
                                             BIGNUM *y);

/** Encodes an EC_KEY public key to an allocated octet string
 *  \param  key    key to encode
 *  \param  form   point conversion form
 *  \param  pbuf   returns pointer to allocated buffer
 *  \param  len    length of the memory buffer
 *  \param  ctx    BN_CTX object (optional)
 *  \return the length of the encoded octet string or 0 if an error occurred
 */

size_t EC_KEY_key2buf(const EC_KEY *key, point_conversion_form_t form,
                      unsigned char **pbuf, BN_CTX *ctx);

/** Decodes a EC_KEY public key from a octet string
 *  \param  key    key to decode
 *  \param  buf    memory buffer with the encoded ec point
 *  \param  len    length of the encoded ec point
 *  \param  ctx    BN_CTX object (optional)
 *  \return 1 on success and 0 if an error occurred
 */

int EC_KEY_oct2key(EC_KEY *key, const unsigned char *buf, size_t len,
                   BN_CTX *ctx);

/** Decodes an EC_KEY private key from an octet string
 *  \param  key    key to decode
 *  \param  buf    memory buffer with the encoded private key
 *  \param  len    length of the encoded key
 *  \return 1 on success and 0 if an error occurred
 */

int EC_KEY_oct2priv(EC_KEY *key, const unsigned char *buf, size_t len);

/** Encodes a EC_KEY private key to an octet string
 *  \param  key    key to encode
 *  \param  buf    memory buffer for the result. If NULL the function returns
 *                 required buffer size.
 *  \param  len    length of the memory buffer
 *  \return the length of the encoded octet string or 0 if an error occurred
 */

size_t EC_KEY_priv2oct(const EC_KEY *key, unsigned char *buf, size_t len);

/** Encodes an EC_KEY private key to an allocated octet string
 *  \param  key    key to encode
 *  \param  pbuf   returns pointer to allocated buffer
 *  \return the length of the encoded octet string or 0 if an error occurred
 */

size_t EC_KEY_priv2buf(const EC_KEY *eckey, unsigned char **pbuf);

/********************************************************************/
/*        de- and encoding functions for SEC1 ECPrivateKey          */
/********************************************************************/

/** Decodes a private key from a memory buffer.
 *  \param  key  a pointer to a EC_KEY object which should be used (or NULL)
 *  \param  in   pointer to memory with the DER encoded private key
 *  \param  len  length of the DER encoded private key
 *  \return the decoded private key or NULL if an error occurred.
 */
EC_KEY *d2i_ECPrivateKey(EC_KEY **key, const unsigned char **in, long len);

/** Encodes a private key object and stores the result in a buffer.
 *  \param  key  the EC_KEY object to encode
 *  \param  out  the buffer for the result (if NULL the function returns number
 *               of bytes needed).
 *  \return 1 on success and 0 if an error occurred.
 */
int i2d_ECPrivateKey(EC_KEY *key, unsigned char **out);

/********************************************************************/
/*        de- and encoding functions for EC parameters              */
/********************************************************************/

/** Decodes ec parameter from a memory buffer.
 *  \param  key  a pointer to a EC_KEY object which should be used (or NULL)
 *  \param  in   pointer to memory with the DER encoded ec parameters
 *  \param  len  length of the DER encoded ec parameters
 *  \return a EC_KEY object with the decoded parameters or NULL if an error
 *          occurred.
 */
EC_KEY *d2i_ECParameters(EC_KEY **key, const unsigned char **in, long len);

/** Encodes ec parameter and stores the result in a buffer.
 *  \param  key  the EC_KEY object with ec parameters to encode
 *  \param  out  the buffer for the result (if NULL the function returns number
 *               of bytes needed).
 *  \return 1 on success and 0 if an error occurred.
 */
int i2d_ECParameters(EC_KEY *key, unsigned char **out);

/********************************************************************/
/*         de- and encoding functions for EC public key             */
/*         (octet string, not DER -- hence 'o2i' and 'i2o')         */
/********************************************************************/

/** Decodes a ec public key from a octet string.
 *  \param  key  a pointer to a EC_KEY object which should be used
 *  \param  in   memory buffer with the encoded public key
 *  \param  len  length of the encoded public key
 *  \return EC_KEY object with decoded public key or NULL if an error
 *          occurred.
 */
EC_KEY *o2i_ECPublicKey(EC_KEY **key, const unsigned char **in, long len);

/** Encodes a ec public key in an octet string.
 *  \param  key  the EC_KEY object with the public key
 *  \param  out  the buffer for the result (if NULL the function returns number
 *               of bytes needed).
 *  \return 1 on success and 0 if an error occurred
 */
int i2o_ECPublicKey(const EC_KEY *key, unsigned char **out);

/** Prints out the ec parameters on human readable form.
 *  \param  bp   BIO object to which the information is printed
 *  \param  key  EC_KEY object
 *  \return 1 on success and 0 if an error occurred
 */
int ECParameters_print(BIO *bp, const EC_KEY *key);

/** Prints out the contents of a EC_KEY object
 *  \param  bp   BIO object to which the information is printed
 *  \param  key  EC_KEY object
 *  \param  off  line offset
 *  \return 1 on success and 0 if an error occurred
 */
int EC_KEY_print(BIO *bp, const EC_KEY *key, int off);

# ifndef OPENSSL_NO_STDIO
/** Prints out the ec parameters on human readable form.
 *  \param  fp   file descriptor to which the information is printed
 *  \param  key  EC_KEY object
 *  \return 1 on success and 0 if an error occurred
 */
int ECParameters_print_fp(FILE *fp, const EC_KEY *key);

/** Prints out the contents of a EC_KEY object
 *  \param  fp   file descriptor to which the information is printed
 *  \param  key  EC_KEY object
 *  \param  off  line offset
 *  \return 1 on success and 0 if an error occurred
 */
int EC_KEY_print_fp(FILE *fp, const EC_KEY *key, int off);

# endif

const EC_KEY_METHOD *EC_KEY_OpenSSL(void);
const EC_KEY_METHOD *EC_KEY_get_default_method(void);
void EC_KEY_set_default_method(const EC_KEY_METHOD *meth);
const EC_KEY_METHOD *EC_KEY_get_method(const EC_KEY *key);
int EC_KEY_set_method(EC_KEY *key, const EC_KEY_METHOD *meth);
EC_KEY *EC_KEY_new_method(ENGINE *engine);

int ECDH_KDF_X9_62(unsigned char *out, size_t outlen,
                   const unsigned char *Z, size_t Zlen,
                   const unsigned char *sinfo, size_t sinfolen,
                   const EVP_MD *md);

int ECDH_compute_key(void *out, size_t outlen, const EC_POINT *pub_key,
                     const EC_KEY *ecdh,
                     void *(*KDF) (const void *in, size_t inlen,
                                   void *out, size_t *outlen));

typedef struct ECDSA_SIG_st ECDSA_SIG;

/** Allocates and initialize a ECDSA_SIG structure
 *  \return pointer to a ECDSA_SIG structure or NULL if an error occurred
 */
ECDSA_SIG *ECDSA_SIG_new(void);

/** frees a ECDSA_SIG structure
 *  \param  sig  pointer to the ECDSA_SIG structure
 */
void ECDSA_SIG_free(ECDSA_SIG *sig);

/** DER encode content of ECDSA_SIG object (note: this function modifies *pp
 *  (*pp += length of the DER encoded signature)).
 *  \param  sig  pointer to the ECDSA_SIG object
 *  \param  pp   pointer to a unsigned char pointer for the output or NULL
 *  \return the length of the DER encoded ECDSA_SIG object or 0
 */
int i2d_ECDSA_SIG(const ECDSA_SIG *sig, unsigned char **pp);

/** Decodes a DER encoded ECDSA signature (note: this function changes *pp
 *  (*pp += len)).
 *  \param  sig  pointer to ECDSA_SIG pointer (may be NULL)
 *  \param  pp   memory buffer with the DER encoded signature
 *  \param  len  length of the buffer
 *  \return pointer to the decoded ECDSA_SIG structure (or NULL)
 */
ECDSA_SIG *d2i_ECDSA_SIG(ECDSA_SIG **sig, const unsigned char **pp, long len);

/** Accessor for r and s fields of ECDSA_SIG
 *  \param  sig  pointer to ECDSA_SIG pointer
 *  \param  pr   pointer to BIGNUM pointer for r (may be NULL)
 *  \param  ps   pointer to BIGNUM pointer for s (may be NULL)
 */
void ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps);

/** Setter for r and s fields of ECDSA_SIG
 *  \param  sig  pointer to ECDSA_SIG pointer
 *  \param  r    pointer to BIGNUM for r (may be NULL)
 *  \param  s    pointer to BIGNUM for s (may be NULL)
 */
int ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s);

/** Computes the ECDSA signature of the given hash value using
 *  the supplied private key and returns the created signature.
 *  \param  dgst      pointer to the hash value
 *  \param  dgst_len  length of the hash value
 *  \param  eckey     EC_KEY object containing a private EC key
 *  \return pointer to a ECDSA_SIG structure or NULL if an error occurred
 */
ECDSA_SIG *ECDSA_do_sign(const unsigned char *dgst, int dgst_len,
                         EC_KEY *eckey);

/** Computes ECDSA signature of a given hash value using the supplied
 *  private key (note: sig must point to ECDSA_size(eckey) bytes of memory).
 *  \param  dgst     pointer to the hash value to sign
 *  \param  dgstlen  length of the hash value
 *  \param  kinv     BIGNUM with a pre-computed inverse k (optional)
 *  \param  rp       BIGNUM with a pre-computed rp value (optional),
 *                   see ECDSA_sign_setup
 *  \param  eckey    EC_KEY object containing a private EC key
 *  \return pointer to a ECDSA_SIG structure or NULL if an error occurred
 */
ECDSA_SIG *ECDSA_do_sign_ex(const unsigned char *dgst, int dgstlen,
                            const BIGNUM *kinv, const BIGNUM *rp,
                            EC_KEY *eckey);

/** Verifies that the supplied signature is a valid ECDSA
 *  signature of the supplied hash value using the supplied public key.
 *  \param  dgst      pointer to the hash value
 *  \param  dgst_len  length of the hash value
 *  \param  sig       ECDSA_SIG structure
 *  \param  eckey     EC_KEY object containing a public EC key
 *  \return 1 if the signature is valid, 0 if the signature is invalid
 *          and -1 on error
 */
int ECDSA_do_verify(const unsigned char *dgst, int dgst_len,
                    const ECDSA_SIG *sig, EC_KEY *eckey);

/** Precompute parts of the signing operation
 *  \param  eckey  EC_KEY object containing a private EC key
 *  \param  ctx    BN_CTX object (optional)
 *  \param  kinv   BIGNUM pointer for the inverse of k
 *  \param  rp     BIGNUM pointer for x coordinate of k * generator
 *  \return 1 on success and 0 otherwise
 */
int ECDSA_sign_setup(EC_KEY *eckey, BN_CTX *ctx, BIGNUM **kinv, BIGNUM **rp);

/** Computes ECDSA signature of a given hash value using the supplied
 *  private key (note: sig must point to ECDSA_size(eckey) bytes of memory).
 *  \param  type     this parameter is ignored
 *  \param  dgst     pointer to the hash value to sign
 *  \param  dgstlen  length of the hash value
 *  \param  sig      memory for the DER encoded created signature
 *  \param  siglen   pointer to the length of the returned signature
 *  \param  eckey    EC_KEY object containing a private EC key
 *  \return 1 on success and 0 otherwise
 */
int ECDSA_sign(int type, const unsigned char *dgst, int dgstlen,
               unsigned char *sig, unsigned int *siglen, EC_KEY *eckey);

/** Computes ECDSA signature of a given hash value using the supplied
 *  private key (note: sig must point to ECDSA_size(eckey) bytes of memory).
 *  \param  type     this parameter is ignored
 *  \param  dgst     pointer to the hash value to sign
 *  \param  dgstlen  length of the hash value
 *  \param  sig      buffer to hold the DER encoded signature
 *  \param  siglen   pointer to the length of the returned signature
 *  \param  kinv     BIGNUM with a pre-computed inverse k (optional)
 *  \param  rp       BIGNUM with a pre-computed rp value (optional),
 *                   see ECDSA_sign_setup
 *  \param  eckey    EC_KEY object containing a private EC key
 *  \return 1 on success and 0 otherwise
 */
int ECDSA_sign_ex(int type, const unsigned char *dgst, int dgstlen,
                  unsigned char *sig, unsigned int *siglen,
                  const BIGNUM *kinv, const BIGNUM *rp, EC_KEY *eckey);

/** Verifies that the given signature is valid ECDSA signature
 *  of the supplied hash value using the specified public key.
 *  \param  type     this parameter is ignored
 *  \param  dgst     pointer to the hash value
 *  \param  dgstlen  length of the hash value
 *  \param  sig      pointer to the DER encoded signature
 *  \param  siglen   length of the DER encoded signature
 *  \param  eckey    EC_KEY object containing a public EC key
 *  \return 1 if the signature is valid, 0 if the signature is invalid
 *          and -1 on error
 */
int ECDSA_verify(int type, const unsigned char *dgst, int dgstlen,
                 const unsigned char *sig, int siglen, EC_KEY *eckey);

/** Returns the maximum length of the DER encoded signature
 *  \param  eckey  EC_KEY object
 *  \return numbers of bytes required for the DER encoded signature
 */
int ECDSA_size(const EC_KEY *eckey);

/********************************************************************/
/*  EC_KEY_METHOD constructors, destructors, writers and accessors  */
/********************************************************************/

EC_KEY_METHOD *EC_KEY_METHOD_new(const EC_KEY_METHOD *meth);
void EC_KEY_METHOD_free(EC_KEY_METHOD *meth);
void EC_KEY_METHOD_set_init(EC_KEY_METHOD *meth,
                            int (*init)(EC_KEY *key),
                            void (*finish)(EC_KEY *key),
                            int (*copy)(EC_KEY *dest, const EC_KEY *src),
                            int (*set_group)(EC_KEY *key, const EC_GROUP *grp),
                            int (*set_private)(EC_KEY *key,
                                               const BIGNUM *priv_key),
                            int (*set_public)(EC_KEY *key,
                                              const EC_POINT *pub_key));

void EC_KEY_METHOD_set_keygen(EC_KEY_METHOD *meth,
                              int (*keygen)(EC_KEY *key));

void EC_KEY_METHOD_set_compute_key(EC_KEY_METHOD *meth,
                                   int (*ckey)(unsigned char **psec,
                                               size_t *pseclen,
                                               const EC_POINT *pub_key,
                                               const EC_KEY *ecdh));

void EC_KEY_METHOD_set_sign(EC_KEY_METHOD *meth,
                            int (*sign)(int type, const unsigned char *dgst,
                                        int dlen, unsigned char *sig,
                                        unsigned int *siglen,
                                        const BIGNUM *kinv, const BIGNUM *r,
                                        EC_KEY *eckey),
                            int (*sign_setup)(EC_KEY *eckey, BN_CTX *ctx_in,
                                              BIGNUM **kinvp, BIGNUM **rp),
                            ECDSA_SIG *(*sign_sig)(const unsigned char *dgst,
                                                   int dgst_len,
                                                   const BIGNUM *in_kinv,
                                                   const BIGNUM *in_r,
                                                   EC_KEY *eckey));

void EC_KEY_METHOD_set_verify(EC_KEY_METHOD *meth,
                              int (*verify)(int type, const unsigned
                                            char *dgst, int dgst_len,
                                            const unsigned char *sigbuf,
                                            int sig_len, EC_KEY *eckey),
                              int (*verify_sig)(const unsigned char *dgst,
                                                int dgst_len,
                                                const ECDSA_SIG *sig,
                                                EC_KEY *eckey));

void EC_KEY_METHOD_get_init(EC_KEY_METHOD *meth,
                            int (**pinit)(EC_KEY *key),
                            void (**pfinish)(EC_KEY *key),
                            int (**pcopy)(EC_KEY *dest, const EC_KEY *src),
                            int (**pset_group)(EC_KEY *key,
                                               const EC_GROUP *grp),
                            int (**pset_private)(EC_KEY *key,
                                                 const BIGNUM *priv_key),
                            int (**pset_public)(EC_KEY *key,
                                                const EC_POINT *pub_key));

void EC_KEY_METHOD_get_keygen(EC_KEY_METHOD *meth,
                              int (**pkeygen)(EC_KEY *key));

void EC_KEY_METHOD_get_compute_key(EC_KEY_METHOD *meth,
                                   int (**pck)(unsigned char **psec,
                                               size_t *pseclen,
                                               const EC_POINT *pub_key,
                                               const EC_KEY *ecdh));

void EC_KEY_METHOD_get_sign(EC_KEY_METHOD *meth,
                            int (**psign)(int type, const unsigned char *dgst,
                                          int dlen, unsigned char *sig,
                                          unsigned int *siglen,
                                          const BIGNUM *kinv, const BIGNUM *r,
                                          EC_KEY *eckey),
                            int (**psign_setup)(EC_KEY *eckey, BN_CTX *ctx_in,
                                                BIGNUM **kinvp, BIGNUM **rp),
                            ECDSA_SIG *(**psign_sig)(const unsigned char *dgst,
                                                     int dgst_len,
                                                     const BIGNUM *in_kinv,
                                                     const BIGNUM *in_r,
                                                     EC_KEY *eckey));

void EC_KEY_METHOD_get_verify(EC_KEY_METHOD *meth,
                              int (**pverify)(int type, const unsigned
                                              char *dgst, int dgst_len,
                                              const unsigned char *sigbuf,
                                              int sig_len, EC_KEY *eckey),
                              int (**pverify_sig)(const unsigned char *dgst,
                                                  int dgst_len,
                                                  const ECDSA_SIG *sig,
                                                  EC_KEY *eckey));

# define ECParameters_dup(x) ASN1_dup_of(EC_KEY,i2d_ECParameters,d2i_ECParameters,x)

# ifndef __cplusplus
#  if defined(__SUNPRO_C)
#   if __SUNPRO_C >= 0x520
#    pragma error_messages (default,E_ARRAY_OF_INCOMPLETE_NONAME,E_ARRAY_OF_INCOMPLETE)
#   endif
#  endif
# endif

# define EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, nid) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_PARAMGEN|EVP_PKEY_OP_KEYGEN, \
                                EVP_PKEY_CTRL_EC_PARAMGEN_CURVE_NID, nid, NULL)

# define EVP_PKEY_CTX_set_ec_param_enc(ctx, flag) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_PARAMGEN|EVP_PKEY_OP_KEYGEN, \
                                EVP_PKEY_CTRL_EC_PARAM_ENC, flag, NULL)

# define EVP_PKEY_CTX_set_ecdh_cofactor_mode(ctx, flag) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_DERIVE, \
                                EVP_PKEY_CTRL_EC_ECDH_COFACTOR, flag, NULL)

# define EVP_PKEY_CTX_get_ecdh_cofactor_mode(ctx) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_DERIVE, \
                                EVP_PKEY_CTRL_EC_ECDH_COFACTOR, -2, NULL)

# define EVP_PKEY_CTX_set_ecdh_kdf_type(ctx, kdf) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_DERIVE, \
                                EVP_PKEY_CTRL_EC_KDF_TYPE, kdf, NULL)

# define EVP_PKEY_CTX_get_ecdh_kdf_type(ctx) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_DERIVE, \
                                EVP_PKEY_CTRL_EC_KDF_TYPE, -2, NULL)

# define EVP_PKEY_CTX_set_ecdh_kdf_md(ctx, md) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_DERIVE, \
                                EVP_PKEY_CTRL_EC_KDF_MD, 0, (void *)md)

# define EVP_PKEY_CTX_get_ecdh_kdf_md(ctx, pmd) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_DERIVE, \
                                EVP_PKEY_CTRL_GET_EC_KDF_MD, 0, (void *)pmd)

# define EVP_PKEY_CTX_set_ecdh_kdf_outlen(ctx, len) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_DERIVE, \
                                EVP_PKEY_CTRL_EC_KDF_OUTLEN, len, NULL)

# define EVP_PKEY_CTX_get_ecdh_kdf_outlen(ctx, plen) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_DERIVE, \
                        EVP_PKEY_CTRL_GET_EC_KDF_OUTLEN, 0, (void *)plen)

# define EVP_PKEY_CTX_set0_ecdh_kdf_ukm(ctx, p, plen) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_DERIVE, \
                                EVP_PKEY_CTRL_EC_KDF_UKM, plen, (void *)p)

# define EVP_PKEY_CTX_get0_ecdh_kdf_ukm(ctx, p) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_EC, \
                                EVP_PKEY_OP_DERIVE, \
                                EVP_PKEY_CTRL_GET_EC_KDF_UKM, 0, (void *)p)

# define EVP_PKEY_CTRL_EC_PARAMGEN_CURVE_NID             (EVP_PKEY_ALG_CTRL + 1)
# define EVP_PKEY_CTRL_EC_PARAM_ENC                      (EVP_PKEY_ALG_CTRL + 2)
# define EVP_PKEY_CTRL_EC_ECDH_COFACTOR                  (EVP_PKEY_ALG_CTRL + 3)
# define EVP_PKEY_CTRL_EC_KDF_TYPE                       (EVP_PKEY_ALG_CTRL + 4)
# define EVP_PKEY_CTRL_EC_KDF_MD                         (EVP_PKEY_ALG_CTRL + 5)
# define EVP_PKEY_CTRL_GET_EC_KDF_MD                     (EVP_PKEY_ALG_CTRL + 6)
# define EVP_PKEY_CTRL_EC_KDF_OUTLEN                     (EVP_PKEY_ALG_CTRL + 7)
# define EVP_PKEY_CTRL_GET_EC_KDF_OUTLEN                 (EVP_PKEY_ALG_CTRL + 8)
# define EVP_PKEY_CTRL_EC_KDF_UKM                        (EVP_PKEY_ALG_CTRL + 9)
# define EVP_PKEY_CTRL_GET_EC_KDF_UKM                    (EVP_PKEY_ALG_CTRL + 10)
/* KDF types */
# define EVP_PKEY_ECDH_KDF_NONE                          1
# define EVP_PKEY_ECDH_KDF_X9_62                         2

/* BEGIN ERROR CODES */
/*
 * The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */

int ERR_load_EC_strings(void);

/* Error codes for the EC functions. */

/* Function codes. */
# define EC_F_BN_TO_FELEM                                 224
# define EC_F_D2I_ECPARAMETERS                            144
# define EC_F_D2I_ECPKPARAMETERS                          145
# define EC_F_D2I_ECPRIVATEKEY                            146
# define EC_F_DO_EC_KEY_PRINT                             221
# define EC_F_ECDH_CMS_DECRYPT                            238
# define EC_F_ECDH_CMS_SET_SHARED_INFO                    239
# define EC_F_ECDH_COMPUTE_KEY                            246
# define EC_F_ECDH_SIMPLE_COMPUTE_KEY                     257
# define EC_F_ECDSA_DO_SIGN_EX                            251
# define EC_F_ECDSA_DO_VERIFY                             252
# define EC_F_ECDSA_SIGN_EX                               254
# define EC_F_ECDSA_SIGN_SETUP                            248
# define EC_F_ECDSA_SIG_NEW                               265
# define EC_F_ECDSA_VERIFY                                253
# define EC_F_ECKEY_PARAM2TYPE                            223
# define EC_F_ECKEY_PARAM_DECODE                          212
# define EC_F_ECKEY_PRIV_DECODE                           213
# define EC_F_ECKEY_PRIV_ENCODE                           214
# define EC_F_ECKEY_PUB_DECODE                            215
# define EC_F_ECKEY_PUB_ENCODE                            216
# define EC_F_ECKEY_TYPE2PARAM                            220
# define EC_F_ECPARAMETERS_PRINT                          147
# define EC_F_ECPARAMETERS_PRINT_FP                       148
# define EC_F_ECPKPARAMETERS_PRINT                        149
# define EC_F_ECPKPARAMETERS_PRINT_FP                     150
# define EC_F_ECP_NISTZ256_GET_AFFINE                     240
# define EC_F_ECP_NISTZ256_MULT_PRECOMPUTE                243
# define EC_F_ECP_NISTZ256_POINTS_MUL                     241
# define EC_F_ECP_NISTZ256_PRE_COMP_NEW                   244
# define EC_F_ECP_NISTZ256_WINDOWED_MUL                   242
# define EC_F_ECX_KEY_OP                                  266
# define EC_F_ECX_PRIV_ENCODE                             267
# define EC_F_ECX_PUB_ENCODE                              268
# define EC_F_EC_ASN1_GROUP2CURVE                         153
# define EC_F_EC_ASN1_GROUP2FIELDID                       154
# define EC_F_EC_GF2M_MONTGOMERY_POINT_MULTIPLY           208
# define EC_F_EC_GF2M_SIMPLE_GROUP_CHECK_DISCRIMINANT     159
# define EC_F_EC_GF2M_SIMPLE_GROUP_SET_CURVE              195
# define EC_F_EC_GF2M_SIMPLE_OCT2POINT                    160
# define EC_F_EC_GF2M_SIMPLE_POINT2OCT                    161
# define EC_F_EC_GF2M_SIMPLE_POINT_GET_AFFINE_COORDINATES 162
# define EC_F_EC_GF2M_SIMPLE_POINT_SET_AFFINE_COORDINATES 163
# define EC_F_EC_GF2M_SIMPLE_SET_COMPRESSED_COORDINATES   164
# define EC_F_EC_GFP_MONT_FIELD_DECODE                    133
# define EC_F_EC_GFP_MONT_FIELD_ENCODE                    134
# define EC_F_EC_GFP_MONT_FIELD_MUL                       131
# define EC_F_EC_GFP_MONT_FIELD_SET_TO_ONE                209
# define EC_F_EC_GFP_MONT_FIELD_SQR                       132
# define EC_F_EC_GFP_MONT_GROUP_SET_CURVE                 189
# define EC_F_EC_GFP_NISTP224_GROUP_SET_CURVE             225
# define EC_F_EC_GFP_NISTP224_POINTS_MUL                  228
# define EC_F_EC_GFP_NISTP224_POINT_GET_AFFINE_COORDINATES 226
# define EC_F_EC_GFP_NISTP256_GROUP_SET_CURVE             230
# define EC_F_EC_GFP_NISTP256_POINTS_MUL                  231
# define EC_F_EC_GFP_NISTP256_POINT_GET_AFFINE_COORDINATES 232
# define EC_F_EC_GFP_NISTP521_GROUP_SET_CURVE             233
# define EC_F_EC_GFP_NISTP521_POINTS_MUL                  234
# define EC_F_EC_GFP_NISTP521_POINT_GET_AFFINE_COORDINATES 235
# define EC_F_EC_GFP_NIST_FIELD_MUL                       200
# define EC_F_EC_GFP_NIST_FIELD_SQR                       201
# define EC_F_EC_GFP_NIST_GROUP_SET_CURVE                 202
# define EC_F_EC_GFP_SIMPLE_GROUP_CHECK_DISCRIMINANT      165
# define EC_F_EC_GFP_SIMPLE_GROUP_SET_CURVE               166
# define EC_F_EC_GFP_SIMPLE_MAKE_AFFINE                   102
# define EC_F_EC_GFP_SIMPLE_OCT2POINT                     103
# define EC_F_EC_GFP_SIMPLE_POINT2OCT                     104
# define EC_F_EC_GFP_SIMPLE_POINTS_MAKE_AFFINE            137
# define EC_F_EC_GFP_SIMPLE_POINT_GET_AFFINE_COORDINATES  167
# define EC_F_EC_GFP_SIMPLE_POINT_SET_AFFINE_COORDINATES  168
# define EC_F_EC_GFP_SIMPLE_SET_COMPRESSED_COORDINATES    169
# define EC_F_EC_GROUP_CHECK                              170
# define EC_F_EC_GROUP_CHECK_DISCRIMINANT                 171
# define EC_F_EC_GROUP_COPY                               106
# define EC_F_EC_GROUP_GET_CURVE_GF2M                     172
# define EC_F_EC_GROUP_GET_CURVE_GFP                      130
# define EC_F_EC_GROUP_GET_DEGREE                         173
# define EC_F_EC_GROUP_GET_ECPARAMETERS                   261
# define EC_F_EC_GROUP_GET_ECPKPARAMETERS                 262
# define EC_F_EC_GROUP_GET_PENTANOMIAL_BASIS              193
# define EC_F_EC_GROUP_GET_TRINOMIAL_BASIS                194
# define EC_F_EC_GROUP_NEW                                108
# define EC_F_EC_GROUP_NEW_BY_CURVE_NAME                  174
# define EC_F_EC_GROUP_NEW_FROM_DATA                      175
# define EC_F_EC_GROUP_NEW_FROM_ECPARAMETERS              263
# define EC_F_EC_GROUP_NEW_FROM_ECPKPARAMETERS            264
# define EC_F_EC_GROUP_SET_CURVE_GF2M                     176
# define EC_F_EC_GROUP_SET_CURVE_GFP                      109
# define EC_F_EC_GROUP_SET_GENERATOR                      111
# define EC_F_EC_KEY_CHECK_KEY                            177
# define EC_F_EC_KEY_COPY                                 178
# define EC_F_EC_KEY_GENERATE_KEY                         179
# define EC_F_EC_KEY_NEW                                  182
# define EC_F_EC_KEY_NEW_METHOD                           245
# define EC_F_EC_KEY_OCT2PRIV                             255
# define EC_F_EC_KEY_PRINT                                180
# define EC_F_EC_KEY_PRINT_FP                             181
# define EC_F_EC_KEY_PRIV2OCT                             256
# define EC_F_EC_KEY_SET_PUBLIC_KEY_AFFINE_COORDINATES    229
# define EC_F_EC_KEY_SIMPLE_CHECK_KEY                     258
# define EC_F_EC_KEY_SIMPLE_OCT2PRIV                      259
# define EC_F_EC_KEY_SIMPLE_PRIV2OCT                      260
# define EC_F_EC_POINTS_MAKE_AFFINE                       136
# define EC_F_EC_POINT_ADD                                112
# define EC_F_EC_POINT_CMP                                113
# define EC_F_EC_POINT_COPY                               114
# define EC_F_EC_POINT_DBL                                115
# define EC_F_EC_POINT_GET_AFFINE_COORDINATES_GF2M        183
# define EC_F_EC_POINT_GET_AFFINE_COORDINATES_GFP         116
# define EC_F_EC_POINT_GET_JPROJECTIVE_COORDINATES_GFP    117
# define EC_F_EC_POINT_INVERT                             210
# define EC_F_EC_POINT_IS_AT_INFINITY                     118
# define EC_F_EC_POINT_IS_ON_CURVE                        119
# define EC_F_EC_POINT_MAKE_AFFINE                        120
# define EC_F_EC_POINT_NEW                                121
# define EC_F_EC_POINT_OCT2POINT                          122
# define EC_F_EC_POINT_POINT2OCT                          123
# define EC_F_EC_POINT_SET_AFFINE_COORDINATES_GF2M        185
# define EC_F_EC_POINT_SET_AFFINE_COORDINATES_GFP         124
# define EC_F_EC_POINT_SET_COMPRESSED_COORDINATES_GF2M    186
# define EC_F_EC_POINT_SET_COMPRESSED_COORDINATES_GFP     125
# define EC_F_EC_POINT_SET_JPROJECTIVE_COORDINATES_GFP    126
# define EC_F_EC_POINT_SET_TO_INFINITY                    127
# define EC_F_EC_PRE_COMP_NEW                             196
# define EC_F_EC_WNAF_MUL                                 187
# define EC_F_EC_WNAF_PRECOMPUTE_MULT                     188
# define EC_F_I2D_ECPARAMETERS                            190
# define EC_F_I2D_ECPKPARAMETERS                          191
# define EC_F_I2D_ECPRIVATEKEY                            192
# define EC_F_I2O_ECPUBLICKEY                             151
# define EC_F_NISTP224_PRE_COMP_NEW                       227
# define EC_F_NISTP256_PRE_COMP_NEW                       236
# define EC_F_NISTP521_PRE_COMP_NEW                       237
# define EC_F_O2I_ECPUBLICKEY                             152
# define EC_F_OLD_EC_PRIV_DECODE                          222
# define EC_F_OSSL_ECDH_COMPUTE_KEY                       247
# define EC_F_OSSL_ECDSA_SIGN_SIG                         249
# define EC_F_OSSL_ECDSA_VERIFY_SIG                       250
# define EC_F_PKEY_ECX_DERIVE                             269
# define EC_F_PKEY_EC_CTRL                                197
# define EC_F_PKEY_EC_CTRL_STR                            198
# define EC_F_PKEY_EC_DERIVE                              217
# define EC_F_PKEY_EC_KEYGEN                              199
# define EC_F_PKEY_EC_PARAMGEN                            219
# define EC_F_PKEY_EC_SIGN                                218

/* Reason codes. */
# define EC_R_ASN1_ERROR                                  115
# define EC_R_BAD_SIGNATURE                               156
# define EC_R_BIGNUM_OUT_OF_RANGE                         144
# define EC_R_BUFFER_TOO_SMALL                            100
# define EC_R_COORDINATES_OUT_OF_RANGE                    146
# define EC_R_CURVE_DOES_NOT_SUPPORT_ECDH                 160
# define EC_R_CURVE_DOES_NOT_SUPPORT_SIGNING              159
# define EC_R_D2I_ECPKPARAMETERS_FAILURE                  117
# define EC_R_DECODE_ERROR                                142
# define EC_R_DISCRIMINANT_IS_ZERO                        118
# define EC_R_EC_GROUP_NEW_BY_NAME_FAILURE                119
# define EC_R_FIELD_TOO_LARGE                             143
# define EC_R_GF2M_NOT_SUPPORTED                          147
# define EC_R_GROUP2PKPARAMETERS_FAILURE                  120
# define EC_R_I2D_ECPKPARAMETERS_FAILURE                  121
# define EC_R_INCOMPATIBLE_OBJECTS                        101
# define EC_R_INVALID_ARGUMENT                            112
# define EC_R_INVALID_COMPRESSED_POINT                    110
# define EC_R_INVALID_COMPRESSION_BIT                     109
# define EC_R_INVALID_CURVE                               141
# define EC_R_INVALID_DIGEST                              151
# define EC_R_INVALID_DIGEST_TYPE                         138
# define EC_R_INVALID_ENCODING                            102
# define EC_R_INVALID_FIELD                               103
# define EC_R_INVALID_FORM                                104
# define EC_R_INVALID_GROUP_ORDER                         122
# define EC_R_INVALID_KEY                                 116
# define EC_R_INVALID_OUTPUT_LENGTH                       161
# define EC_R_INVALID_PEER_KEY                            133
# define EC_R_INVALID_PENTANOMIAL_BASIS                   132
# define EC_R_INVALID_PRIVATE_KEY                         123
# define EC_R_INVALID_TRINOMIAL_BASIS                     137
# define EC_R_KDF_PARAMETER_ERROR                         148
# define EC_R_KEYS_NOT_SET                                140
# define EC_R_MISSING_PARAMETERS                          124
# define EC_R_MISSING_PRIVATE_KEY                         125
# define EC_R_NEED_NEW_SETUP_VALUES                       157
# define EC_R_NOT_A_NIST_PRIME                            135
# define EC_R_NOT_IMPLEMENTED                             126
# define EC_R_NOT_INITIALIZED                             111
# define EC_R_NO_PARAMETERS_SET                           139
# define EC_R_NO_PRIVATE_VALUE                            154
# define EC_R_OPERATION_NOT_SUPPORTED                     152
# define EC_R_PASSED_NULL_PARAMETER                       134
# define EC_R_PEER_KEY_ERROR                              149
# define EC_R_PKPARAMETERS2GROUP_FAILURE                  127
# define EC_R_POINT_ARITHMETIC_FAILURE                    155
# define EC_R_POINT_AT_INFINITY                           106
# define EC_R_POINT_IS_NOT_ON_CURVE                       107
# define EC_R_RANDOM_NUMBER_GENERATION_FAILED             158
# define EC_R_SHARED_INFO_ERROR                           150
# define EC_R_SLOT_FULL                                   108
# define EC_R_UNDEFINED_GENERATOR                         113
# define EC_R_UNDEFINED_ORDER                             128
# define EC_R_UNKNOWN_GROUP                               129
# define EC_R_UNKNOWN_ORDER                               114
# define EC_R_UNSUPPORTED_FIELD                           131
# define EC_R_WRONG_CURVE_PARAMETERS                      145
# define EC_R_WRONG_ORDER                                 130

#  ifdef  __cplusplus
}
#  endif
# endif
#endif
