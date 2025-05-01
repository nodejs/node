Fetching composite algorithms and using them - adding the bits still missing
============================================================================

Quick background
----------------

We currently support - at least in the public libcrypto API - explicitly
fetching composite algorithms (such as AES-128-CBC or HMAC-SHA256), and
using them in most cases.  In some cases (symmetric ciphers), our providers
also provide them.

However, there is one class of algorithms where the support for *using*
explicitly fetched algorithms is lacking: asymmetric algorithms.

For a longer background and explanation, see
[Background / tl;dr](#background-tldr) at the end of this design.

Public API - Add variants of `EVP_PKEY_CTX` initializers
--------------------------------------------------------

As far as this design is concerned, these API sets are affected:

- SIGNATURE (DigestSign and DigestVerify)
- ASYM_CIPHER
- KEYEXCH

The proposal is to add these functions:

``` C
EVP_DigestSignInit_ex2(EVP_PKEY_CTX **pctx,
                       EVP_SIGNATURE *sig, EVP_PKEY *pkey,
                       OSSL_LIB_CTX *libctx, const OSSL_PARAM params[]);
EVP_DigestVerifyInit_ex2(EVP_PKEY_CTX **pctx,
                         EVP_SIGNATURE *sig, EVP_PKEY *pkey,
                         OSSL_LIB_CTX *libctx, const OSSL_PARAM params[]);

int EVP_PKEY_encrypt_init_ex2(EVP_PKEY_CTX *ctx, EVP_ASYM_CIPHER *asymciph,
                              const OSSL_PARAM params[]);
int EVP_PKEY_decrypt_init_ex2(EVP_PKEY_CTX *ctx, EVP_ASYM_CIPHER *asymciph,
                              const OSSL_PARAM params[]);

int EVP_PKEY_derive_init_ex2(EVP_PKEY_CTX *ctx, EVP_KEYEXCH *exchange,
                             const OSSL_PARAM params[]);
```

Because `EVP_SIGNATURE`, `EVP_ASYM_CIPHER` and `EVP_KEYEXCH` aren't limited
to composite algorithms, these functions can be used just as well with
explicit fetches of simple algorithms, say "RSA".  In that case, the caller
will need to pass necessary auxiliary parameters through the `OSSL_PARAM` or
a call to a corresponding `set_params` function.

Requirements on the providers
-----------------------------

Because it's not immediately obvious from a composite algorithm name what
key type it requires / supports, at least in code, allowing the use of an
explicitly fetched implementation of a composite algorithm requires that
providers cooperate by declaring what key type is required / supported by
each algorithm.

For non-composite operation algorithms (like "RSA"), this is not necessary,
see the fallback strategies below.

There are two ways this could be implemented:

1.  through an added provider function that would work like keymgmt's
    `query_operation_name` function, but would return a key type name
    instead:

    ``` C
    # define OSSL_FUNC_SIGNATURE_QUERY_KEY_TYPE         26
    OSSL_CORE_MAKE_FUNC(const char *, signature_query_key_type, (void))

    # define OSSL_FUNC ASYM_CIPHER_QUERY_KEY_TYPE       12
    OSSL_CORE_MAKE_FUNC(const char *, asym_cipher_query_key_type, (void))

    # define OSSL_FUNC_KEYEXCH_QUERY_KEY_TYPE           11
    OSSL_CORE_MAKE_FUNC(const char *, keyexch_query_key_type, (void))
    ```

2.  through a gettable `OSSL_PARAM`, using the param identity "keytype"

Fallback strategies
-------------------

Because existing providers haven't been updated to declare composite
algorithms, or to respond to the key type query, some fallback strategies
will be needed to find out if the `EVP_PKEY` key type is possible to use
with the fetched algorithm:

-   Check if the fetched operation name matches the key type (keymgmt name)
    of the `EVP_PKEY` that's involved in the operation.  For example, this
    is useful when someone fetched the `EVP_SIGNATURE` "RSA".
-   Check if the fetched algorithm name matches the name returned by the
    keymgmt's `query_operation_name` function.  For example, this is useful
    when someone fetched the `EVP_SIGNATURE` "ECDSA", for which the key type
    to use is "EC".
-   libcrypto currently has knowledge of some composite algorithm names and
    what they are composed of, accessible with `OBJ_find_sigid_algs` and
    similar functionality.  This knowledge is regarded legacy, but can be
    used to figure out the key type.

If none of these strategies work out, the operation initialization should
fail.

These strategies have their limitations, but the built-in legacy knowledge
we currently have in libcrypto should be enough to cover most bases.

-----

-----

Background / tl;dr
------------------

### What is a composite algorithm?

A composite algorithm is an algorithm that's composed of more than one other
algorithm.  In OpenSSL parlance with a focus on signatures, they have been
known as "sigalgs", but this is really broader than just signature algorithms.
Examples are:

-   AES-128-CBC
-   hmacWithSHA256
-   sha256WithRSAEncryption

### The connection with AlgorithmIdentifiers

AlgorithmIdentifier is an ASN.1 structure that defines an algorithm as an
OID, along with parameters that should be passed to that algorithm.

It is expected that an application should be able to take that OID and
fetch it directly, after conversion to string form (either a name if the
application or libcrypto happens to know it, or the OID itself in canonical
numerical form).  To enable this, explicit fetching is necessary.

### What we have today

As a matter of fact, we already have built-in support for fetching
composite algorithms, although our providers do not fully participate in
that support, and *most of the time*, we also have public APIs to use the
fetched result, commonly known as support for explicit fetching.

The idea is that providers can declare the different compositions of a base
algorithm in the `OSSL_ALGORITHM` array, each pointing to different
`OSSL_DISPATCH` tables, which would in turn refer to pretty much the same
functions, apart from the constructor function.

For example, we already do this with symmetric ciphers.

Another example, which we could implement in our providers today, would be
compositions of HMAC:

``` C
static const OSSL_ALGORITHM deflt_macs[] = {
    /* ... */
    { "HMAC-SHA1:hmacWithSHA1:1.2.840.113549.2.7",
      "provider=default", ossl_hmac_sha1_functions },
    { "HMAC-SHA224:hmacWithSHA224:1.2.840.113549.2.8",
      "provider=default", ossl_hmac_sha224_functions },
    { "HMAC-SHA256:hmacWithSHA256:1.2.840.113549.2.9",
      "provider=default", ossl_hmac_sha256_functions },
    { "HMAC-SHA384:hmacWithSHA384:1.2.840.113549.2.10",
      "provider=default", ossl_hmac_sha384_functions },
    { "HMAC-SHA512:hmacWithSHA512:1.2.840.113549.2.11",
      "provider=default", ossl_hmac_sha512_functions },
    /* ... */
```

### What we don't have today

There are some classes of algorithms for which we have no support for using
the result of explicit fetching.  So for example, while it's possible for a
provider to declare composite algorithms through the `OSSL_ALGORITHM` array,
there's currently no way for an application to use them.

This all revolves around asymmetric algorithms, where we currently only
support implicit fetching.

This is hurtful in multiple ways:

-   It fails the provider authors in terms being able to consistently
    declare all algorithms through `OSSL_ALGORITHM` arrays.
-   It fails the applications in terms of being able to fetch algorithms and
    use the result.
-   It fails discoverability, for example through the `openssl list`
    command.
