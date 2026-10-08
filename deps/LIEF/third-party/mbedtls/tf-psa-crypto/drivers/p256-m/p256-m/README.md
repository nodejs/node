*This is the original README for the p256-m repository. Please note that as
only a subset of p256-m's files are present in Mbed TLS, this README may refer
to files that are not present/relevant here.*

p256-m is a minimalistic implementation of ECDH and ECDSA on NIST P-256,
especially suited to constrained 32-bit environments. It's written in standard
C, with optional bits of assembly for Arm Cortex-M and Cortex-A CPUs.

Its design is guided by the following goals in this order:

1. correctness & security;
2. low code size & RAM usage;
3. runtime performance.

Most cryptographic implementations care more about speed than footprint, and
some might even risk weakening security for more speed. p256-m was written
because I wanted to see what happened when reversing the usual emphasis.

The result is a full implementation of ECDH and ECDSA in **less than 3KiB of
code**, using **less than 768 bytes of RAM**, with comparable performance
to existing implementations (see below) - in less than 700 LOC.

_Contents of this Readme:_

- [Correctness](#correctness)
- [Security](#security)
- [Code size](#code-size)
- [RAM usage](#ram-usage)
- [Runtime performance](#runtime-performance)
- [Comparison with other implementations](#comparison-with-other-implementations)
- [Design overview](#design-overview)
- [Notes about other curves](#notes-about-other-curves)
- [Notes about other platforms](#notes-about-other-platforms)

## Correctness

**API design:**

- The API is minimal: only 4 public functions.
- Each public function fully validates its inputs and returns specific errors.
- The API uses arrays of octets for all input and output.

**Testing:**

- p256-m is validated against multiple test vectors from various RFCs and
  NIST.
- In addition, crafted inputs are used for negative testing and to reach
  corner cases.
- Two test suites are provided: one for closed-box testing (using only the
  public API), one for open-box testing (for unit-testing internal functions,
and reaching more error cases by exploiting knowledge of how the RNG is used).
- The resulting branch coverage is maximal: closed-box testing reaches all
  branches except four; three of them are reached by open-box testing using a
rigged RNG; the last branch could only be reached by computing a discrete log
on P-256... See `coverage.sh`.
- Testing also uses dynamic analysis: valgrind, ASan, MemSan, UBSan.

**Code quality:**

- The code is standard C99; it builds without warnings with `clang
  -Weverything` and `gcc -Wall -Wextra -pedantic`.
- The code is small and well documented, including internal APIs: with the
  header file, it's less than 700 lines of code, and more lines of comments
than of code.
- However it _has not been reviewed_ independently so far, as this is a
  personal project.

**Short Weierstrass pitfalls:**

Its has been [pointed out](https://safecurves.cr.yp.to/) that the NIST curves,
and indeed all Short Weierstrass curves, have a number of pitfalls including
risk for the implementation to:

- "produce incorrect results for some rare curve points" - this is avoided by
  carefully checking the validity domain of formulas used throughout the code;
- "leak secret data when the input isn't a curve point" - this is avoided by
  validating that points lie on the curve every time a point is deserialized.

## Security

In addition to the above correctness claims, p256-m has the following
properties:

- it has no branch depending (even indirectly) on secret data;
- it has no memory access depending (even indirectly) on secret data.

These properties are checked using valgrind and MemSan with the ideas
behind [ctgrind](https://github.com/agl/ctgrind), see `consttime.sh`.

In addition to avoiding branches and memory accesses depending on secret data,
p256-m also avoid instructions (or library functions) whose execution time
depends on the value of operands on cores of interest. Namely, it never uses
integer division, and for multiplication by default it only uses 16x16->32 bit
unsigned multiplication. On cores which have a constant-time 32x32->64 bit
unsigned multiplication instruction, the symbol `MUL64_IS_CONSTANT_TIME` can
be defined by the user at compile-time to take advantage of it in order to
improve performance and code size. (On Cortex-M and Cortex-A cores wtih GCC or
Clang this is not necessary, since inline assembly is used instead.)

As a result, p256-m should be secure against the following classes of attackers:

1. attackers who can only manipulate the input and observe the output;
2. attackers who can also measure the total computation time of the operation;
3. attackers who can also observe and manipulate micro-architectural features
   such as the cache or branch predictor with arbitrary precision.

However, p256-m makes no attempt to protect against:

4. passive physical attackers who can record traces of physical emissions
   (power, EM, sound) of the CPU while it manipulates secrets;
5. active physical attackers who can also inject faults in the computation.

(Note: p256-m should actually be secure against SPA, by virtue of being fully
constant-flow, but is not expected to resist any other physical attack.)

**Warning:** p256-m requires an externally-provided RNG function. If that
function is not cryptographically secure, then neither is p256-m's key
generation or ECDSA signature generation.

_Note:_ p256-m also follows best practices such as securely erasing secret
data on the stack before returning.

## Code size

Compiled with
[ARM-GCC 9](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads),
with `-mthumb -Os`, here are samples of code sizes reached on selected cores:

- Cortex-M0: 2988 bytes
- Cortex-M4: 2900 bytes
- Cortex-A7: 2924 bytes

Clang was also tried but tends to generate larger code (by about 10%). For
details, see `sizes.sh`.

**What's included:**

- Full input validation and (de)serialisation of input/outputs to/from bytes.
- Cleaning up secret values from the stack before returning from a function.
- The code has no dependency on libc functions or the toolchain's runtime
  library (such as helpers for long multiply); this can be checked for the
Arm-GCC toolchain with the `deps.sh` script.

**What's excluded:**

- A secure RNG function needs to be provided externally, see
  `p256_generate_random()` in `p256-m.h`.

## RAM usage

p256-m doesn't use any dynamic memory (on the heap), only the stack. Here's
how much stack is used by each of its 4 public functions on selected cores:

| Function                  | Cortex-M0 | Cortex-M4 | Cortex-A7 |
| ------------------------- | --------: | --------: | --------: |
| `p256_gen_keypair`        |       608 |       564 |       564 |
| `p256_ecdh_shared_secret` |       640 |       596 |       596 |
| `p256_ecdsa_sign`         |       664 |       604 |       604 |
| `p256_ecdsa_verify`       |       752 |       700 |       700 |

For details, see `stack.sh`, `wcs.py` and `libc.msu` (the above figures assume
that the externally-provided RNG function uses at most 384 bytes of stack).

## Runtime performance

Here are the timings of each public function in milliseconds measured on
platforms based on a selection of cores:

- Cortex-M0 at  48 MHz: STM32F091 board running Mbed OS 6
- Cortex-M4 at 100 MHz: STM32F411 board running Mbed OS 6
- Cortex-A7 at 900 MHz: Raspberry Pi 2B running Raspbian Buster

| Function                  | Cortex-M0 | Cortex-M4 | Cortex-A7 |
| ------------------------- | --------: | --------: | --------: |
| `p256_gen_keypair`        |       921 |       145 |        11 |
| `p256_ecdh_shared_secret` |       922 |       144 |        11 |
| `p256_ecdsa_sign`         |       990 |       155 |        12 |
| `p256_ecdsa_verify`       |      1976 |       309 |        24 |
| Sum of the above          |      4809 |       753 |        59 |

The sum of these operations corresponds to a TLS handshake using ECDHE-ECDSA
with mutual authentication based on raw public keys or directly-trusted
certificates (otherwise, add one 'verify' for each link in the peer's
certificate chain).

_Note_: the above figures where obtained by compiling with GCC, which is able
to use inline assembly. Without that inline assembly (22 lines for Cortex-M0,
1 line for Cortex-M4), the code would be roughly 2 times slower on those
platforms. (The effect is much less important on the Cortex-A7 core.)

For details, see `bench.sh`, `benchmark.c` and `on-target-benchmark/`.

## Comparison with other implementations

The most relevant/convenient implementation for comparisons is
[TinyCrypt](https://github.com/intel/tinycrypt), as it's also a standalone
implementation of ECDH and ECDSA on P-256 only, that also targets constrained
devices. Other implementations tend to implement many curves and build on a
shared bignum/MPI module (possibly also supporting RSA), which makes fair
comparisons less convenient.

The scripts used for TinyCrypt measurements are available in [this
branch](https://github.com/mpg/tinycrypt/tree/measurements), based on version
0.2.8.

**Code size**

| Core      | p256-m | TinyCrypt |
| --------- | -----: | --------: |
| Cortex-M0 |   2988 |      6134 |
| Cortex-M4 |   2900 |      5934 |
| Cortex-A7 |   2924 |      5934 |

**RAM usage**

TinyCrypto also uses no heap, only the stack. Here's the RAM used by each
operation on a Cortex-M0 core:

| operation          | p256-m | TinyCrypt |
| ------------------ | -----: | --------: |
| key generation     |    608 |       824 |
| ECDH shared secret |    640 |       728 |
| ECDSA sign         |    664 |       880 |
| ECDSA verify       |    752 |       824 |

On a Cortex-M4 or Cortex-A7 core (identical numbers):

| operation          | p256-m | TinyCrypt |
| ------------------ | -----: | --------: |
| key generation     |    564 |       796 |
| ECDH shared secret |    596 |       700 |
| ECDSA sign         |    604 |       844 |
| ECDSA verify       |    700 |       808 |

**Runtime performance**

Here are the timings of each operation in milliseconds measured on
platforms based on a selection of cores:

_Cortex-M0_ at  48 MHz: STM32F091 board running Mbed OS 6

| Operation          | p256-m | TinyCrypt |
| ------------------ | -----: | --------: |
| Key generation     |    921 |       979 |
| ECDH shared secret |    922 |       975 |
| ECDSA sign         |    990 |      1009 |
| ECDSA verify       |   1976 |      1130 |
| Sum of those 4     |   4809 |      4093 |

_Cortex-M4_ at 100 MHz: STM32F411 board running Mbed OS 6

| Operation          | p256-m | TinyCrypt |
| ------------------ | -----: | --------: |
| Key generation     |    145 |       178 |
| ECDH shared secret |    144 |       177 |
| ECDSA sign         |    155 |       188 |
| ECDSA verify       |    309 |       210 |
| Sum of those 4     |    753 |       753 |

_Cortex-A7_ at 900 MHz: Raspberry Pi 2B running Raspbian Buster

| Operation          | p256-m | TinyCrypt |
| ------------------ | -----: | --------: |
| Key generation     |     11 |        13 |
| ECDH shared secret |     11 |        13 |
| ECDSA sign         |     12 |        14 |
| ECDSA verify       |     24 |        15 |
| Sum of those 4     |     59 |        55 |

_64-bit Intel_ (i7-6500U at 2.50GHz) laptop running Ubuntu 20.04

Note: results in microseconds (previous benchmarks in milliseconds)

| Operation          | p256-m | TinyCrypt |
| ------------------ | -----: | --------: |
| Key generation     |   1060 |      1627 |
| ECDH shared secret |   1060 |      1611 |
| ECDSA sign         |   1136 |      1712 |
| ECDSA verify       |   2279 |      1888 |
| Sum of those 4     |   5535 |      6838 |

**Other differences**

- While p256-m fully validates all inputs, Tinycrypt's ECDH shared secret
  function doesn't include validation of the peer's public key, which should be
done separately by the user for static ECDH (there are attacks [when users
forget](https://link.springer.com/chapter/10.1007/978-3-319-24174-6_21)).
- The two implementations have slightly different security characteristics:
  p256-m is fully constant-time from the ground up so should be more robust
than TinyCrypt against powerful local attackers (such as an untrusted OS
attacking a secure enclave); on the other hand TinyCrypt includes coordinate
randomisation which protects against some passive physical attacks (such as
DPA, see Table 3, column C9 of [this
paper](https://www.esat.kuleuven.be/cosic/publications/article-2293.pdf#page=12)),
which p256-m completely ignores.
- TinyCrypt's code looks like it could easily be expanded to support other
  curves, while p256-m has much more hard-coded to minimize code size (see
"Notes about other curves" below).
- TinyCrypt uses a specialised routine for reduction modulo the curve prime,
  exploiting its structure as a Solinas prime, which should be faster than the
generic Montgomery reduction used by p256-m, but other factors appear to
compensate for that.
- TinyCrypt uses Co-Z Jacobian formulas for point operation, which should be
  faster (though a bit larger) than the mixed affine-Jacobian formulas
used by p256-m, but again other factors appear to compensate for that.
- p256-m uses bits of inline assembly for 64-bit multiplication on the
  platforms used for benchmarking, while TinyCrypt uses only C (and the
compiler's runtime library).
- TinyCrypt uses a specialised routine based on Shamir's trick for
  ECDSA verification, which gives much better performance than the generic
code that p256-m uses in order to minimize code size.

## Design overview

The implementation is contained in a single file to keep most functions static
and allow for more optimisations. It is organized in multiple layers:

- Fixed-width multi-precision arithmetic
- Fixed-width modular arithmetic
- Operations on curve points
- Operations with scalars
- The public API

**Multi-precision arithmetic.**

Large integers are represented as arrays of `uint32_t` limbs. When carries may
occur, casts to `uint64_t` are used to nudge the compiler towards using the
CPU's carry flag. When overflow may occur, functions return a carry flag.

This layer contains optional assembly for Cortex-M and Cortex-A cores, for the
internal `u32_muladd64()` function, as well as two pure C versions of this
function, depending on whether `MUL64_IS_CONSTANT_TIME`.

This layer's API consists of:

- addition, subtraction;
- multiply-and-add, shift by one limb (for Montgomery multiplication);
- conditional assignment, assignment of a small value;
- comparison of two values for equality, comparison to 0 for equality;
- (de)serialization as big-endian arrays of bytes.

**Modular arithmetic.**

All modular operations are done in the Montgomery domain, that is x is
represented by `x * 2^256 mod m`; integers need to be converted to that domain
before computations, and back from it afterwards. Montgomery constants
associated to the curve's p and n are pre-computed and stored in static
structures.

Modular inversion is computed using Fermat's little theorem to get
constant-time behaviour with respect to the value being inverted.

This layer's API consists of:

- the curve's constants p and n (and associated Montgomery constants);
- modular addition, subtraction, multiplication, and inversion;
- assignment of a small value;
- conversion to/from Montgomery domain;
- (de)serialization to/from bytes with integrated range checking and
  Montgomery domain conversion.

**Operations on curve points.**

Curve points are represented using either affine or Jacobian coordinates;
affine coordinates are extended to represent 0 as (0,0). Individual
coordinates are always in the Montgomery domain.

Not all formulas associated with affine or Jacobian coordinates are complete;
great care is taken to document and satisfy each function's pre-conditions.

This layer's API consists of:

- curve constants: b from the equation, the base point's coordinates;
- point validity check (on the curve and not 0);
- Jacobian to affine coordinate conversion;
- point doubling in Jacobian coordinates (complete formulas);
- point addition in mixed affine-Jacobian coordinates (P not in {0, Q, -Q});
- point addition-or-doubling in affine coordinates (leaky version, only used
  for ECDSA verify where all data is public);
- (de)serialization to/from bytes with integrated validity checking

**Scalar operations.**

The crucial function here is scalar multiplication. It uses a signed binary
ladder, which is a variant of the good old double-and-add algorithm where an
addition/subtraction is performed at each step. Again, care is taken to make
sure the pre-conditions for the addition formulas are always satisfied. The
signed binary ladder only works if the scalar is odd; this is ensured by
negating both the scalar (mod n) and the input point if necessary.

This layer's API consists of:

- scalar multiplication
- de-serialization from bytes with integrated range checking
- generation of a scalar and its associated public key

**Public API.**

This layer builds on the others, but unlike them, all inputs and outputs are
byte arrays. Key generation and ECDH shared secret computation are thin
wrappers around internal functions, just taking care of format conversions and
errors. The ECDSA functions have more non-trivial logic.

This layer's API consists of:

- key-pair generation
- ECDH shared secret computation
- ECDSA signature creation
- ECDSA signature verification

**Testing.**

A self-contained, straightforward, pure-Python implementation was first
produced as a warm-up and to help check intermediate values. Test vectors from
various sources are embedded and used to validate the implementation.

This implementation, `p256.py`, is used by a second Python script,
`gen-test-data.py`, to generate additional data for both positive and negative
testing, available from a C header file, that is then used by the closed-box
and open-box test programs.

p256-m can be compiled with extra instrumentation to mark secret data and
allow either valgrind or MemSan to check that no branch or memory access
depends on it (even indirectly). Macros are defined for this purpose near the
top of the file.

**Tested platforms.**

There are 4 versions of the internal function `u32_muladd64`: two assembly
versions, for Cortex-M/A cores with or without the DSP extension, and two
pure-C versions, depending on whether `MUL64_IS_CONSTANT_TIME`.

Tests are run on the following platforms:

- `make` on x64 tests the pure-C version without `MUL64_IS_CONSTANT_TIME`
  (with Clang).
- `./consttime.sh` on x64 tests both pure-C versions (with Clang).
- `make` on Arm v7-A (Raspberry Pi 2) tests the Arm-DSP assembly version (with
  Clang).
- `on-target-*box` on boards based on Cortex-M0 and M4 cores test both
  assembly versions (with GCC).

In addition:

- `sizes.sh` builds the code for three Arm cores with GCC and Clang.
- `deps.sh` checks for external dependencies with GCC.

## Notes about other curves

It should be clear that minimal code size can only be reached by specializing
the implementation to the curve at hand. Here's a list of things in the
implementation that are specific to the NIST P-256 curve, and how the
implementation could be changed to expand to other curves, layer by layer (see
"Design Overview" above).

**Fixed-width multi-precision arithmetic:**

- The number of limbs is hard-coded to 8. For other 256-bit curves, nothing to
  change. For a curve of another size, hard-code to another value. For multiple
curves of various sizes, add a parameter to each function specifying the
number of limbs; when declaring arrays, always use the maximum number of
limbs.

**Fixed-width modular arithmetic:**

- The values of the curve's constant p and n, and their associated Montgomery
  constants, are hard-coded. For another curve, just hard-code the new constants.
For multiple other curves, define all the constants, and from this layer's API
only keep the functions that already accept a `mod` parameter (that is, remove
convenience functions `m256_xxx_p()`).
- The number of limbs is again hard-coded to 8. See above, but it order to
  support multiple sizes there is no need to add a new parameter to functions
in this layer: the existing `mod` parameter can include the number of limbs as
well.

**Operations on curve points:**

- The values of the curve's constants b (constant term from the equation) and
  gx, gy (coordinates of the base point) are hard-coded. For another curve,
  hard-code the other values. For multiple curves, define each curve's value and
add a "curve id" parameter to all functions in this layer.
- The value of the curve's constant a is implicitly hard-coded to `-3` by using
  a standard optimisation to save one multiplication in the first step of
`point_double()`. For curves that don't have a == -3, replace that with the
normal computation.
- The fact that b != 0 in the curve equation is used indirectly, to ensure
  that (0, 0) is not a point on the curve and re-use that value to represent
the point 0. As far as I know, all Short Weierstrass curves standardized so
far have b != 0.
- The shape of the curve is assumed to be Short Weierstrass. For other curve
  shapes (Montgomery, (twisted) Edwards), this layer would probably look very
different (both implementation and API).

**Scalar operations:**

- If multiple curves are to be supported, all function in this layer need to
  gain a new "curve id" parameter.
- This layer assumes that the bit size of the curve's order n is the same as
  that of the modulus p. This is true of most curves standardized so far, the
only exception being secp224k1. If that curve were to be supported, the
representation of `n` and scalars would need adapting to allow for an extra
limb.
- The bit size of the curve's order is hard-coded in `scalar_mult()`. For
  multiple curves, this should be deduced from the "curve id" parameter.
- The `scalar_mult()` function exploits the fact that the second least
  significant bit of the curve's order n is set in order to avoid a special
case. For curve orders that don't meet this criterion, we can just handle that
special case (multiplication by +-2) separately (always compute that and
conditionally assign it to the result).
- The shape of the curve is again assumed to be Short Weierstrass. For other curve
  shapes (Montgomery, (twisted) Edwards), this layer would probably have a
very different implementation.

**Public API:**

- For multiple curves, all functions in this layer would need to gain a "curve
  id" parameter and handle variable-sized input/output.
- The shape of the curve is again assumed to be Short Weierstrass. For other curve
  shapes (Montgomery, (twisted) Edwards), the ECDH API would probably look
quite similar (with differences in the size of public keys), but the ECDSA API
wouldn't apply and an EdDSA API would look pretty different.

## Notes about other platforms

While p256-m is standard C99, it is written with constrained 32-bit platforms
in mind and makes a few assumptions about the platform:

- The types `uint8_t`, `uint16_t`, `uint32_t` and `uint64_t` exist.
- 32-bit unsigned addition and subtraction with carry are constant time.
- 16x16->32-bit unsigned multiplication is available and constant time.

Also, on platforms on which 64-bit addition and subtraction with carry, or
even 64x64->128-bit multiplication, are available, p256-m makes no use of
them, though they could significantly improve performance.

This could be improved by replacing uses of arrays of `uint32_t` with a
defined type throughout the internal APIs, and then on 64-bit platforms define
that type to be an array of `uint64_t` instead, and making the obvious
adaptations in the multi-precision arithmetic layer.

Finally, the optional assembly code (which boosts performance by a factor 2 on
tested Cortex-M CPUs, while slightly reducing code size and stack usage) is
currently only available with compilers that support GCC's extended asm
syntax (which includes GCC and Clang).
