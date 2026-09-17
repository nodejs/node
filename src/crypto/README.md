# Node.js `src/crypto` documentation

Welcome. You've found your way to the Node.js native crypto subsystem.

Do not be afraid.

While crypto may be a dark, mysterious, and foreboding subject; and while
this directory may be filled with many `*.h` and `*.cc` files, finding
your way around is not too difficult. And I can promise you that a Gru
will not jump out of the shadows and eat you (well, "promise" may be a
bit too strong, a Gru may jump out of the shadows and eat you if you
live in a place where such things are possible).

## Finding your way around

All of the code in this directory is structured into units organized by
function or crypto protocol.

The following provide generalized utility declarations that are used throughout
the various other crypto files and other parts of Node.js:

* `crypto_util.h` / `crypto_util.cc` (Core crypto definitions)
* `crypto_common.h` / `crypto_common.cc` (Shared TLS utility functions)
* `crypto_bio.h` / `crypto_bio.cc` (Custom OpenSSL i/o implementation)

Of these, `crypto_util.h` and `crypto_util.cc` are the most important, as
they provide the core declarations and utility functions used most extensively
throughout the rest of the code.

The rest of the files are structured by their function, as detailed in the
following table:

| File (\*.h/\*.cc) | Description                                                          |
| ----------------- | -------------------------------------------------------------------- |
| `crypto_aes`      | AES Cipher support.                                                  |
| `crypto_argon2`   | Argon2 key / bit generation implementation.                          |
| `crypto_cipher`   | General Encryption/Decryption utilities.                             |
| `crypto_context`  | Implementation of the `SecureContext` object.                        |
| `crypto_dh`       | Diffie-Hellman Key Agreement implementation.                         |
| `crypto_dsa`      | DSA (Digital Signature) Key Generation functions.                    |
| `crypto_ec`       | Elliptic-curve cryptography implementation.                          |
| `crypto_hash`     | Basic hash (e.g. SHA-256) functions.                                 |
| `crypto_hkdf`     | HKDF (Key derivation) implementation.                                |
| `crypto_hmac`     | HMAC implementations.                                                |
| `crypto_keygen`   | Secret and asymmetric key generation jobs.                           |
| `crypto_keys`     | Utilities for using and generating secret, private, and public keys. |
| `crypto_mac`      | Provider-generic MAC implementations.                                |
| `crypto_pbkdf2`   | PBKDF2 key / bit generation implementation.                          |
| `crypto_pqc`      | Post-quantum algorithm enumeration.                                  |
| `crypto_rsa`      | RSA Key Generation functions.                                        |
| `crypto_scrypt`   | Scrypt key / bit generation implementation.                          |
| `crypto_sig`      | General digital signature and verification utilities.                |
| `crypto_spkac`    | Netscape SPKAC certificate utilities.                                |
| `crypto_ssl`      | Implementation of the `SSLWrap` object.                              |
| `crypto_timing`   | Implementation of the TimingSafeEqual.                               |
| `crypto_x509`     | X.509 certificate parsing and validation.                            |

When new crypto protocols are added, they will be added into their own
`crypto_` `*.h` and `*.cc` files.

## Helpful concepts

Node.js currently uses OpenSSL to provide its crypto substructure.
(Some custom Node.js distributions -- such as Electron -- use BoringSSL
instead.)

This section aims to explain some of the utilities that have been
provided to make working with the OpenSSL APIs a bit easier.

### The ncrypto boundary

[`deps/ncrypto`](../../deps/ncrypto) provides the OpenSSL and BoringSSL
wrappers used by this subsystem. Put backend adaptation, algorithm metadata,
and reusable cryptographic operations there. Keep JavaScript argument
validation, public API policy and errors, V8 objects, and crypto job integration
in `src/crypto` and `lib/internal/crypto`.

For provider operations, query the supported algorithms and parameters through
ncrypto. Version guards are still needed when a C API is unavailable in older
headers or when handling a known version-specific behavior.

### Pointer types

Most OpenSSL objects need to be explicitly freed when they are no longer
needed. Use the ownership wrappers declared in
[`ncrypto.h`](../../deps/ncrypto/ncrypto.h) to manage their lifetime.

Some wrappers, such as `PKCS8Pointer`, are `DeleteFnPtr` aliases. Others,
including `EVPKeyPointer`, `EVPKeyCtxPointer`, `EVPMDCtxPointer`, `BIOPointer`,
`BignumPointer`, and `ECKeyPointer`, are dedicated classes that also provide
operations on the wrapped state. Their representation can vary by backend;
use their methods to keep that adaptation inside ncrypto.

Examples of these being used are pervasive through the `src/crypto` code.

`HMACCtxPointer` is a dedicated HMAC state wrapper rather than a plain
`DeleteFnPtr` alias. On OpenSSL 3 and later it owns the provider-backed
`EVP_MAC`/`EVP_MAC_CTX` state. On OpenSSL 1.1.1 and BoringSSL it owns the
legacy `HMAC_CTX` state. HMAC call sites should use `HMACCtxPointer::New()`,
`init()`, `update()`, and `digest()`/`digestInto()` so the backend selection
stays contained in ncrypto.

### `ByteSource`

The `ByteSource` class is a helper utility representing a _read-only_ byte
array. Instances can either wrap external ("foreign") data sources, such as
an `ArrayBuffer` (`v8::BackingStore`), or allocated data.

* If a pointer to external data is used to create a `ByteSource`, that pointer
  must remain valid until the `ByteSource` is destroyed.
* If allocated data is used, then it must have been allocated using OpenSSL's
  allocator. It will be freed automatically when the `ByteSource` is destroyed.

### `ArrayBufferOrViewContents`

The `ArrayBufferOrViewContents` class is a helper utility that abstracts
`ArrayBuffer`, `TypedArray`, or `DataView` inputs and provides access to
their underlying data pointers. It is used extensively through `src/crypto`
to make it easier to deal with inputs that allow any `ArrayBuffer`-backed
object.

The lifetime of `ArrayBufferOrViewContents` should not exceed the
lifetime of its input.

### Key objects

Most crypto operations involve the use of keys -- cryptographic inputs
that protect data. There are three general types of keys:

* Secret Keys (Symmetric)
* Public Keys (Asymmetric)
* Private Keys (Asymmetric)

Secret keys consist of a variable number of bytes. They are "symmetrical"
in that the same key used to encrypt data, or generate a signature, must
be used to decrypt or validate that signature. If two people are exchanging
messages encrypted using a secret key, both of them must have access to the
same secret key data.

Public and Private keys always come in pairs. When one is used to encrypt
data or generate a signature, the other is used to decrypt or validate the
signature. The Public key is intended to be shared and can be shared openly.
The Private key must be kept secret and known only to the owner of the key.

The `src/crypto` subsystem uses several objects to represent keys. These
objects are structured in a way to allow key data to be shared across
multiple threads (the Node.js main thread, Worker Threads, and the libuv
threadpool).

Refer to `crypto_keys.h` and `crypto_keys.cc` for all code relating to the
core key objects.

#### Asymmetric key algorithms

Use ncrypto's `KeyAlgorithm` descriptors to identify known algorithms in C++.
They hold canonical algorithm names and metadata, with static lifetime, so
callers can retain descriptor pointers across asynchronous jobs. For example,
`KeyAlgorithm::RSA_PSS` names `RSA-PSS` and `KeyAlgorithm::ML_DSA_44` names
`ML-DSA-44`.

For an existing key, use `key.isA(KeyAlgorithm::RSA_PSS)` or another descriptor.
On OpenSSL 3 and later this uses `EVP_PKEY_is_a()` to recognize provider aliases.
Numeric key IDs are unsuitable for provider-only keys: OpenSSL can return `-1`
for their ID. Numeric adapters for BoringSSL and legacy OpenSSL stay private to
ncrypto.

When a function needs algorithm metadata, use `key.getAlgorithm()`. It returns
a pointer to a static descriptor, or `nullptr` for an empty key or an unrecognized
algorithm. Reuse it for multiple checks within that function. Key-based helpers
resolve the algorithm internally. Keep RSA distinct from RSA-PSS, and EC distinct
from SM2.

`key.rawSeed()` validates seed support and extracts the seed in one operation.
Its result distinguishes an unsupported key type from an unavailable seed.
OKP and AKP JWKs share ncrypto's `key.exportRawJwk()` and
`EVPKeyPointer::NewRawJwk()` operations. Export returns the recognized algorithm
and public/private bytes, selecting seed versus raw private material internally.
Import checks that public bytes match the supplied private material. Node.js
handles the distinct JWK field names, input-name validation, base64url encoding,
JavaScript objects, and errors. RSA and EC retain their component and coordinate
representations; ncrypto handles prime-product validation and affine-coordinate
extraction through `Rsa` and `Ec`.

`Ec::GetCurveId()`, `GetKeyComponents()`, and the raw-export helpers read
provider parameters directly where supported, with backend adaptation in
ncrypto. Keep these operations on the `EVPKeyPointer` when an `ECKeyPointer`
reconstruction is unnecessary.

`key.getKeyTypeName()` returns the descriptor's lowercase public key-type name,
or `nullptr` when the key has no recognized public type. For example, ML-DSA-44
returns `ml-dsa-44`, while SM2 has no public key-type name. These names live in
ncrypto alongside the algorithm metadata.

For construction, use `EVPKeyCtxPointer::NewFromAlgorithm()` with a descriptor,
or `NewFromName()` when a backend algorithm name is needed.
`EVPKeyPointer::NewRawPublic()`, `NewRawPrivate()`, and `NewRawSeed()` take
descriptors too. Use the key's capability helpers for raw formats and signature
contexts; recognizing an algorithm does not establish support for an operation.

`KeyAlgorithm::FromName()` looks up known canonical names case-insensitively
using the same `CaseInsensitiveNameEqual` as the digest, cipher, and MAC caches.
It does not resolve arbitrary provider aliases or establish availability.
`isAvailable()` checks whether the backend can create a context for the algorithm.
Public input validation remains specific to each API: PQC JWK `alg` values use
exact canonical names such as `ML-DSA-44`, while raw imports require exact public
`asymmetricKeyType` values such as `ml-dsa-44`.

The internal JavaScript binding exposes `getPqcKeyTypes()` for the available
known PQC algorithm names in their canonical spelling. Named key generation
passes algorithm names to `NamedKeyPairGenJob`, which resolves the name to a static
`KeyAlgorithm` descriptor. Asymmetric key IDs are not exposed to JavaScript.

Real EC curve and ASN.1/OID NIDs still have their own uses. The EC generation
path keeps Ed/X algorithm descriptors separate from curve NIDs while preserving
the existing accepted curve-name aliases.

#### `KeyObjectData`

`KeyObjectData` is an internal thread-safe structure used to wrap either
an `EVPKeyPointer` (for Public or Private keys) or a `ByteSource` containing
a Secret key. It is the shared backing representation used by `KeyObject`,
`CryptoKey`, and native crypto jobs that operate on key material.

#### `KeyObjectHandle`

`KeyObjectHandle` is the internal JavaScript-visible C++ handle for a
`KeyObjectData`. It exposes operations that internal JavaScript uses to
initialize, inspect, compare, and export key material. Native code passes
`KeyObjectData` across threads and jobs; a `KeyObjectHandle` is created when
JavaScript needs access to those operations and is kept out of user-visible
`KeyObject` own properties.

#### `KeyObject`

A `KeyObject` is the public Node.js-specific API for keys. It extends a
native `NativeKeyObject`, which stores `KeyObjectData` for structured
cloning. The JavaScript constructor caches the known key type in a private
field outside user-visible own properties. When a `KeyObjectHandle` is first
needed, JavaScript replaces that value with a hidden native-backed slot tuple.
Derived metadata, such as symmetric key size and asymmetric key details, is
read from the cached handle and appended lazily to the same private-field
cache.

#### `CryptoKey`

A `CryptoKey` is the Web Crypto API key type. In the Node.js implementation,
public `CryptoKey` instances are backed by a native `NativeCryptoKey`, not by
a `KeyObject`. `NativeCryptoKey` stores the same primary `KeyObjectData`
representation as `KeyObject`, plus the Web Crypto internal slots
(`[[extractable]]`, `[[algorithm]]`, and `[[usages]]`). Normal construction
primes a private JavaScript slot cache from the constructor arguments.
Partially initialized transferred keys populate that cache from the native
slots on first access. For Hybrid KEM `CryptoKey` instances only,
`NativeCryptoKey` may also store a secondary `KeyObjectData` and `seed_data`
used to reconstruct the hybrid key material.

### X.509 certificates

The public `X509Certificate` is backed directly by the native
`X509Certificate` object. JavaScript caches derived certificate properties in
a private array whose final entry is a bitmask of populated slots. Certificates
from another realm use the same cache layout through a private `WeakMap` after
passing the native brand check.

### `CryptoJob`

All operations that are not either Stream-based or single-use functions
are built around the `CryptoJob` class.

A `CryptoJob` encapsulates a single crypto operation that can be
invoked synchronously, asynchronously, or as a Web Crypto API
Promise-based job.

The `CryptoJob` class itself is a C++ template that takes a single
`CryptoJobTraits` struct as a parameter. The `CryptoJobTraits`
provides the implementation detail of the job.

There are (currently) three basic `CryptoJob` specializations:

* `CipherJob` (defined in `src/crypto_cipher.h`) -- Used for
  encrypt and decrypt operations.
* `KeyGenJob` (defined in `src/crypto_keygen.h`) -- Used for
  secret and key pair generation operations.
* `DeriveBitsJob` (defined in `src/crypto_util.h`) -- Used for
  key and byte derivation operations.

Every `CryptoJobTraits` provides two fundamental operations:

* Configuration -- Processes input arguments when a
  `CryptoJob` instance is created.
* Implementation -- Provides the specific implementation of
  the operation.

The Configuration is typically provided by an `AdditionalConfig()`
method, the signature of which is slightly different for each
of the above `CryptoJob` specializations. Despite the signature
differences, the purpose of the `AdditionalConfig()` function
remains the same: to process input arguments and set the properties
on the `CryptoJob`'s parameters object.

The parameters object is specific to each `CryptoJob` type, and
is stored with the `CryptoJob`. It holds all of the inputs that
are used by the Implementation. The inputs held by the parameters
must be threadsafe.

The `AdditionalConfig()` function is always called when the
`CryptoJob` instance is being created.

The Implementation function is unique to each of the `CryptoJob`
specializations and will either be called synchronously within
the current thread or from within the libuv threadpool.

Every `CryptoJob` instance exposes a `run()` function to the
JavaScript layer. When called, `run()` will either dispatch the
job to the libuv threadpool, invoke the Implementation function
synchronously, or return a `Promise` for Web Crypto API jobs. If
invoked synchronously, `run()` will return a JavaScript array.
The first value in the array is either an `Error` or `undefined`.
If the operation was successful, the second value in the array
will contain the result of the operation. Typically, the result
is an `ArrayBuffer`, but certain `CryptoJob` types can alter the
output.

If the `CryptoJob` is processed asynchronously, then the job
must have an `ondone` property whose value is a function that
is invoked when the operation is complete. This function will
be called with two arguments. The first is either an `Error`
or `undefined`, and the second is the result of the operation
if successful.

If the `CryptoJob` is processed as a Web Crypto API job, then
`run()` returns a Promise. Operation-specific failures are
rejected with an `OperationError`, and successful jobs resolve
with the Web Crypto API result shape expected by the JavaScript
implementation.

For `CipherJob` types, the output is always an `ArrayBuffer`.

For `KeyGenJob` types, the output is either a single KeyObject,
or an array containing a Public/Private key pair represented
either as a `KeyObjectHandle` object or a `Buffer`. Web Crypto
API key generation jobs return a `CryptoKey` or a `CryptoKeyPair`
object.

For `DeriveBitsJob` type output is typically an `ArrayBuffer` but
can be other values (`RandomBytesJob` for instance, fills an
input buffer and always returns `undefined`).

### Errors

#### `ThrowCryptoError` and the `THROW_ERR_CRYPTO_*` macros

The `ThrowCryptoError()` is a legacy utility that will throw a
JavaScript exception containing details collected from OpenSSL
about a failed operation. `ThrowCryptoError()` should only be
used when necessary to report low-level OpenSSL failures.

In `node_errors.h`, there are a number of `ERR_CRYPTO_*`
macro definitions that define semantically specific errors.
These can be called from within the C++ code as functions,
like `THROW_ERR_CRYPTO_INVALID_IV(env)`. These methods
should be used to throw JavaScript errors when necessary.

## Crypto API patterns

### Operation mode

All crypto functions in Node.js operate in one of these
modes:

* Synchronous single-call
* Asynchronous single-call
* Web Crypto API Promise-based
* Stream-oriented

It is often possible to perform various operations across
multiple modes. For instance, cipher and decipher operations
can be performed in any of the three modes.

Synchronous single-call operations are always blocking.
They perform their actions immediately.

```js
// Example synchronous single-call operation
const { timingSafeEqual } = require('node:crypto');
const a = new Uint8Array(10);
const b = new Uint8Array(10);
timingSafeEqual(a, b);
```

Asynchronous single-call operations generally perform a
number of synchronous input validation steps, but then
defer the actual crypto-operation work to the libuv threadpool.

```js
// Example asynchronous single-call operation
const { randomFill } = require('node:crypto');
const buf = new Uint8Array(10);
randomFill(buf, (err, buf) => {
  if (err) throw err;
  console.log(buf);
});
```

For the legacy Node.js crypto API, asynchronous single-call
operations use the traditional Node.js callback pattern, as
illustrated in the previous `randomFill()` example. In the
Web Crypto API (accessible via `globalThis.crypto`),
all asynchronous single-call operations are Promise-based.

```js
// Example Web Crypto API asynchronous single-call operation
const { subtle } = globalThis.crypto;

subtle.generateKey({ name: 'HMAC', hash: 'SHA-256', length: 256 }, true, ['sign'])
  .then((key) => {
    console.log(key);
  })
  .catch((error) => {
    console.error('an error occurred', error);
  });
```

In nearly every case, asynchronous single-call operations make use
of the libuv threadpool to perform crypto operations off the main
event loop thread.

Stream-oriented operations use an object to maintain state
over multiple individual synchronous steps. The steps themselves
can be performed over time.

```js
// Example stream-oriented operation
const { createHash } = require('node:crypto');
const hash = createHash('sha256');
setTimeout(() => {
  hash.update('hello world');
  setTimeout(() => {
    console.log(hash.digest());
  }, 1000);
}, 1000);
```
