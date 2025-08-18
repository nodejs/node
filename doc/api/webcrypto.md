# Web Crypto API

<!-- YAML
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ChaCha20-Poly1305 algorithm is now supported.
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: SHA-3 algorithms are now supported.
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: SHAKE algorithms are now supported.
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ML-DSA algorithms are now supported.
  - version:
    - v23.5.0
    - v22.13.0
    - v20.19.3
    pr-url: https://github.com/nodejs/node/pull/56142
    description: Algorithms `Ed25519` and `X25519` are now stable.
  - version:
    - v20.0.0
    - v18.17.0
    pr-url: https://github.com/nodejs/node/pull/46067
    description: Arguments are now coerced and validated as per their WebIDL
      definitions like in other Web Crypto API implementations.
  - version: v19.0.0
    pr-url: https://github.com/nodejs/node/pull/44897
    description: No longer experimental except for the `Ed25519`, `Ed448`,
      `X25519`, and `X448` algorithms.
  - version:
    - v18.4.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/43310
    description: Removed proprietary `'node.keyObject'` import/export format.
  - version:
    - v18.4.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/43310
    description: Removed proprietary `'NODE-DSA'`, `'NODE-DH'`,
      and `'NODE-SCRYPT'` algorithms.
  - version:
    - v18.4.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42507
    description: Added `'Ed25519'`, `'Ed448'`, `'X25519'`, and `'X448'`
      algorithms.
  - version:
    - v18.4.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42507
    description: Removed proprietary `'NODE-ED25519'` and `'NODE-ED448'`
      algorithms.
  - version:
    - v18.4.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42507
    description: Removed proprietary `'NODE-X25519'` and `'NODE-X448'` named
      curves from the `'ECDH'` algorithm.
-->

<!-- introduced_in=v15.0.0 -->

> Stability: 2 - Stable

Node.js provides an implementation of the [Web Crypto API][] standard.

Use `globalThis.crypto` or `require('node:crypto').webcrypto` to access this
module.

```js
const { subtle } = globalThis.crypto;

(async function() {

  const key = await subtle.generateKey({
    name: 'HMAC',
    hash: 'SHA-256',
    length: 256,
  }, true, ['sign', 'verify']);

  const enc = new TextEncoder();
  const message = enc.encode('I love cupcakes');

  const digest = await subtle.sign({
    name: 'HMAC',
  }, key, message);

})();
```

## Modern Algorithms in the Web Cryptography API

> Stability: 1.1 - Active development

Node.js provides an implementation of the following features from the
[Modern Algorithms in the Web Cryptography API](https://wicg.github.io/webcrypto-modern-algos/)
WICG proposal:

Algorithms:

* `'ChaCha20-Poly1305'`
* `'cSHAKE128'`
* `'cSHAKE256'`
* `'ML-DSA-44'`[^openssl35]
* `'ML-DSA-65'`[^openssl35]
* `'ML-DSA-87'`[^openssl35]
* `'SHA3-256'`
* `'SHA3-384'`
* `'SHA3-512'`

Key Formats:

* `'raw-public'`
* `'raw-secret'`
* `'raw-seed'`

Methods:

* [`subtle.getPublicKey()`][]
* [`SubtleCrypto.supports()`][]

## Secure Curves in the Web Cryptography API

> Stability: 1.1 - Active development

Node.js provides an implementation of the following features from the
[Secure Curves in the Web Cryptography API](https://wicg.github.io/webcrypto-secure-curves/)
WICG proposal:

Algorithms:

* `'Ed448'`
* `'X448'`

## Examples

### Generating keys

The {SubtleCrypto} class can be used to generate symmetric (secret) keys
or asymmetric key pairs (public key and private key).

#### AES keys

```js
const { subtle } = globalThis.crypto;

async function generateAesKey(length = 256) {
  const key = await subtle.generateKey({
    name: 'AES-CBC',
    length,
  }, true, ['encrypt', 'decrypt']);

  return key;
}
```

#### ECDSA key pairs

```js
const { subtle } = globalThis.crypto;

async function generateEcKey(namedCurve = 'P-521') {
  const {
    publicKey,
    privateKey,
  } = await subtle.generateKey({
    name: 'ECDSA',
    namedCurve,
  }, true, ['sign', 'verify']);

  return { publicKey, privateKey };
}
```

#### Ed25519/X25519 key pairs

```js
const { subtle } = globalThis.crypto;

async function generateEd25519Key() {
  return subtle.generateKey({
    name: 'Ed25519',
  }, true, ['sign', 'verify']);
}

async function generateX25519Key() {
  return subtle.generateKey({
    name: 'X25519',
  }, true, ['deriveKey']);
}
```

#### HMAC keys

```js
const { subtle } = globalThis.crypto;

async function generateHmacKey(hash = 'SHA-256') {
  const key = await subtle.generateKey({
    name: 'HMAC',
    hash,
  }, true, ['sign', 'verify']);

  return key;
}
```

#### RSA key pairs

```js
const { subtle } = globalThis.crypto;
const publicExponent = new Uint8Array([1, 0, 1]);

async function generateRsaKey(modulusLength = 2048, hash = 'SHA-256') {
  const {
    publicKey,
    privateKey,
  } = await subtle.generateKey({
    name: 'RSASSA-PKCS1-v1_5',
    modulusLength,
    publicExponent,
    hash,
  }, true, ['sign', 'verify']);

  return { publicKey, privateKey };
}
```

### Encryption and decryption

```js
const crypto = globalThis.crypto;

async function aesEncrypt(plaintext) {
  const ec = new TextEncoder();
  const key = await generateAesKey();
  const iv = crypto.getRandomValues(new Uint8Array(16));

  const ciphertext = await crypto.subtle.encrypt({
    name: 'AES-CBC',
    iv,
  }, key, ec.encode(plaintext));

  return {
    key,
    iv,
    ciphertext,
  };
}

async function aesDecrypt(ciphertext, key, iv) {
  const dec = new TextDecoder();
  const plaintext = await crypto.subtle.decrypt({
    name: 'AES-CBC',
    iv,
  }, key, ciphertext);

  return dec.decode(plaintext);
}
```

### Exporting and importing keys

```js
const { subtle } = globalThis.crypto;

async function generateAndExportHmacKey(format = 'jwk', hash = 'SHA-512') {
  const key = await subtle.generateKey({
    name: 'HMAC',
    hash,
  }, true, ['sign', 'verify']);

  return subtle.exportKey(format, key);
}

async function importHmacKey(keyData, format = 'jwk', hash = 'SHA-512') {
  const key = await subtle.importKey(format, keyData, {
    name: 'HMAC',
    hash,
  }, true, ['sign', 'verify']);

  return key;
}
```

### Wrapping and unwrapping keys

```js
const { subtle } = globalThis.crypto;

async function generateAndWrapHmacKey(format = 'jwk', hash = 'SHA-512') {
  const [
    key,
    wrappingKey,
  ] = await Promise.all([
    subtle.generateKey({
      name: 'HMAC', hash,
    }, true, ['sign', 'verify']),
    subtle.generateKey({
      name: 'AES-KW',
      length: 256,
    }, true, ['wrapKey', 'unwrapKey']),
  ]);

  const wrappedKey = await subtle.wrapKey(format, key, wrappingKey, 'AES-KW');

  return { wrappedKey, wrappingKey };
}

async function unwrapHmacKey(
  wrappedKey,
  wrappingKey,
  format = 'jwk',
  hash = 'SHA-512') {

  const key = await subtle.unwrapKey(
    format,
    wrappedKey,
    wrappingKey,
    'AES-KW',
    { name: 'HMAC', hash },
    true,
    ['sign', 'verify']);

  return key;
}
```

### Sign and verify

```js
const { subtle } = globalThis.crypto;

async function sign(key, data) {
  const ec = new TextEncoder();
  const signature =
    await subtle.sign('RSASSA-PKCS1-v1_5', key, ec.encode(data));
  return signature;
}

async function verify(key, signature, data) {
  const ec = new TextEncoder();
  const verified =
    await subtle.verify(
      'RSASSA-PKCS1-v1_5',
      key,
      signature,
      ec.encode(data));
  return verified;
}
```

### Deriving bits and keys

```js
const { subtle } = globalThis.crypto;

async function pbkdf2(pass, salt, iterations = 1000, length = 256) {
  const ec = new TextEncoder();
  const key = await subtle.importKey(
    'raw',
    ec.encode(pass),
    'PBKDF2',
    false,
    ['deriveBits']);
  const bits = await subtle.deriveBits({
    name: 'PBKDF2',
    hash: 'SHA-512',
    salt: ec.encode(salt),
    iterations,
  }, key, length);
  return bits;
}

async function pbkdf2Key(pass, salt, iterations = 1000, length = 256) {
  const ec = new TextEncoder();
  const keyMaterial = await subtle.importKey(
    'raw',
    ec.encode(pass),
    'PBKDF2',
    false,
    ['deriveKey']);
  const key = await subtle.deriveKey({
    name: 'PBKDF2',
    hash: 'SHA-512',
    salt: ec.encode(salt),
    iterations,
  }, keyMaterial, {
    name: 'AES-GCM',
    length,
  }, true, ['encrypt', 'decrypt']);
  return key;
}
```

### Digest

```js
const { subtle } = globalThis.crypto;

async function digest(data, algorithm = 'SHA-512') {
  const ec = new TextEncoder();
  const digest = await subtle.digest(algorithm, ec.encode(data));
  return digest;
}
```

### Checking for runtime algorithm support

[`SubtleCrypto.supports()`][] allows feature detection in Web Crypto API,
which can be used to detect whether a given algorithm identifier
(including its parameters) is supported for the given operation.

This example derives a key from a password using Argon2, if available,
or PBKDF2, otherwise; and then encrypts and decrypts some text with it
using AES-OCB, if available, and AES-GCM, otherwise.

```mjs
const { SubtleCrypto, crypto } = globalThis;

const password = 'correct horse battery staple';
const derivationAlg =
  SubtleCrypto.supports?.('importKey', 'Argon2id') ?
    'Argon2id' :
    'PBKDF2';
const encryptionAlg =
  SubtleCrypto.supports?.('importKey', 'AES-OCB') ?
    'AES-OCB' :
    'AES-GCM';
const passwordKey = await crypto.subtle.importKey(
  derivationAlg === 'Argon2id' ? 'raw-secret' : 'raw',
  new TextEncoder().encode(password),
  derivationAlg,
  false,
  ['deriveKey'],
);
const nonce = crypto.getRandomValues(new Uint8Array(16));
const derivationParams =
  derivationAlg === 'Argon2id' ?
    {
      nonce,
      parallelism: 4,
      memory: 2 ** 21,
      passes: 1,
    } :
    {
      salt: nonce,
      iterations: 100_000,
      hash: 'SHA-256',
    };
const key = await crypto.subtle.deriveKey(
  {
    name: derivationAlg,
    ...derivationParams,
  },
  passwordKey,
  {
    name: encryptionAlg,
    length: 256,
  },
  false,
  ['encrypt', 'decrypt'],
);
const plaintext = 'Hello, world!';
const iv = crypto.getRandomValues(new Uint8Array(16));
const encrypted = await crypto.subtle.encrypt(
  { name: encryptionAlg, iv },
  key,
  new TextEncoder().encode(plaintext),
);
const decrypted = new TextDecoder().decode(await crypto.subtle.decrypt(
  { name: encryptionAlg, iv },
  key,
  encrypted,
));
```

## Algorithm matrix

The table details the algorithms supported by the Node.js Web Crypto API
implementation and the APIs supported for each:

| Algorithm                            | `generateKey` | `exportKey` | `importKey` | `encrypt/decrypt` | `wrapKey/unwrapKey` | `deriveBits/deriveKey` | `sign/verify` | `digest` | `getPublicKey` |
| ------------------------------------ | ------------- | ----------- | ----------- | ----------------- | ------------------- | ---------------------- | ------------- | -------- | -------------- |
| `'AES-CBC'`                          | ✔             | ✔           | ✔           | ✔                 | ✔                   |                        |               |          |                |
| `'AES-CTR'`                          | ✔             | ✔           | ✔           | ✔                 | ✔                   |                        |               |          |                |
| `'AES-GCM'`                          | ✔             | ✔           | ✔           | ✔                 | ✔                   |                        |               |          |                |
| `'AES-KW'`                           | ✔             | ✔           | ✔           |                   | ✔                   |                        |               |          |                |
| `'ChaCha20-Poly1305'`[^modern-algos] | ✔             | ✔           | ✔           | ✔                 | ✔                   |                        |               |          |                |
| `'cSHAKE128'`[^modern-algos]         |               |             |             |                   |                     |                        |               | ✔        |                |
| `'cSHAKE256'`[^modern-algos]         |               |             |             |                   |                     |                        |               | ✔        |                |
| `'ECDH'`                             | ✔             | ✔           | ✔           |                   |                     | ✔                      |               |          | ✔              |
| `'ECDSA'`                            | ✔             | ✔           | ✔           |                   |                     |                        | ✔             |          | ✔              |
| `'Ed25519'`                          | ✔             | ✔           | ✔           |                   |                     |                        | ✔             |          | ✔              |
| `'Ed448'`[^secure-curves]            | ✔             | ✔           | ✔           |                   |                     |                        | ✔             |          | ✔              |
| `'HKDF'`                             |               |             | ✔           |                   |                     | ✔                      |               |          |                |
| `'HMAC'`                             | ✔             | ✔           | ✔           |                   |                     |                        | ✔             |          |                |
| `'ML-DSA-44'`[^modern-algos]         | ✔             | ✔           | ✔           |                   |                     |                        | ✔             |          | ✔              |
| `'ML-DSA-65'`[^modern-algos]         | ✔             | ✔           | ✔           |                   |                     |                        | ✔             |          | ✔              |
| `'ML-DSA-87'`[^modern-algos]         | ✔             | ✔           | ✔           |                   |                     |                        | ✔             |          | ✔              |
| `'PBKDF2'`                           |               |             | ✔           |                   |                     | ✔                      |               |          |                |
| `'RSA-OAEP'`                         | ✔             | ✔           | ✔           | ✔                 | ✔                   |                        |               |          | ✔              |
| `'RSA-PSS'`                          | ✔             | ✔           | ✔           |                   |                     |                        | ✔             |          | ✔              |
| `'RSASSA-PKCS1-v1_5'`                | ✔             | ✔           | ✔           |                   |                     |                        | ✔             |          | ✔              |
| `'SHA-1'`                            |               |             |             |                   |                     |                        |               | ✔        |                |
| `'SHA-256'`                          |               |             |             |                   |                     |                        |               | ✔        |                |
| `'SHA-384'`                          |               |             |             |                   |                     |                        |               | ✔        |                |
| `'SHA-512'`                          |               |             |             |                   |                     |                        |               | ✔        |                |
| `'SHA3-256'`[^modern-algos]          |               |             |             |                   |                     |                        |               | ✔        |                |
| `'SHA3-384'`[^modern-algos]          |               |             |             |                   |                     |                        |               | ✔        |                |
| `'SHA3-512'`[^modern-algos]          |               |             |             |                   |                     |                        |               | ✔        |                |
| `'X25519'`                           | ✔             | ✔           | ✔           |                   |                     | ✔                      |               |          | ✔              |
| `'X448'`[^secure-curves]             | ✔             | ✔           | ✔           |                   |                     | ✔                      |               |          | ✔              |

## Class: `Crypto`

<!-- YAML
added: v15.0.0
-->

`globalThis.crypto` is an instance of the `Crypto`
class. `Crypto` is a singleton that provides access to the remainder of the
crypto API.

### `crypto.subtle`

<!-- YAML
added: v15.0.0
-->

* Type: {SubtleCrypto}

Provides access to the `SubtleCrypto` API.

### `crypto.getRandomValues(typedArray)`

<!-- YAML
added: v15.0.0
-->

* `typedArray` {Buffer|TypedArray}
* Returns: {Buffer|TypedArray}

Generates cryptographically strong random values. The given `typedArray` is
filled with random values, and a reference to `typedArray` is returned.

The given `typedArray` must be an integer-based instance of {TypedArray},
i.e. `Float32Array` and `Float64Array` are not accepted.

An error will be thrown if the given `typedArray` is larger than 65,536 bytes.

### `crypto.randomUUID()`

<!-- YAML
added: v16.7.0
-->

* Returns: {string}

Generates a random [RFC 4122][] version 4 UUID. The UUID is generated using a
cryptographic pseudorandom number generator.

## Class: `CryptoKey`

<!-- YAML
added: v15.0.0
-->

### `cryptoKey.algorithm`

<!-- YAML
added: v15.0.0
-->

<!--lint disable maximum-line-length remark-lint-->

* Type: {KeyAlgorithm|RsaHashedKeyAlgorithm|EcKeyAlgorithm|AesKeyAlgorithm|HmacKeyAlgorithm}

<!--lint enable maximum-line-length remark-lint-->

An object detailing the algorithm for which the key can be used along with
additional algorithm-specific parameters.

Read-only.

### `cryptoKey.extractable`

<!-- YAML
added: v15.0.0
-->

* Type: {boolean}

When `true`, the {CryptoKey} can be extracted using either
`subtleCrypto.exportKey()` or `subtleCrypto.wrapKey()`.

Read-only.

### `cryptoKey.type`

<!-- YAML
added: v15.0.0
-->

* Type: {string} One of `'secret'`, `'private'`, or `'public'`.

A string identifying whether the key is a symmetric (`'secret'`) or
asymmetric (`'private'` or `'public'`) key.

### `cryptoKey.usages`

<!-- YAML
added: v15.0.0
-->

* Type: {string\[]}

An array of strings identifying the operations for which the
key may be used.

The possible usages are:

* `'encrypt'` - The key may be used to encrypt data.
* `'decrypt'` - The key may be used to decrypt data.
* `'sign'` - The key may be used to generate digital signatures.
* `'verify'` - The key may be used to verify digital signatures.
* `'deriveKey'` - The key may be used to derive a new key.
* `'deriveBits'` - The key may be used to derive bits.
* `'wrapKey'` - The key may be used to wrap another key.
* `'unwrapKey'` - The key may be used to unwrap another key.

Valid key usages depend on the key algorithm (identified by
`cryptokey.algorithm.name`).

| Supported Key Algorithm              | `'encrypt'` | `'decrypt'` | `'sign'` | `'verify'` | `'deriveKey'` | `'deriveBits'` | `'wrapKey'` | `'unwrapKey'` |
| ------------------------------------ | ----------- | ----------- | -------- | ---------- | ------------- | -------------- | ----------- | ------------- |
| `'AES-CBC'`                          | ✔           | ✔           |          |            |               |                | ✔           | ✔             |
| `'AES-CTR'`                          | ✔           | ✔           |          |            |               |                | ✔           | ✔             |
| `'AES-GCM'`                          | ✔           | ✔           |          |            |               |                | ✔           | ✔             |
| `'AES-KW'`                           |             |             |          |            |               |                | ✔           | ✔             |
| `'ChaCha20-Poly1305'`[^modern-algos] | ✔           | ✔           |          |            |               |                | ✔           | ✔             |
| `'ECDH'`                             |             |             |          |            | ✔             | ✔              |             |               |
| `'ECDSA'`                            |             |             | ✔        | ✔          |               |                |             |               |
| `'Ed25519'`                          |             |             | ✔        | ✔          |               |                |             |               |
| `'Ed448'`[^secure-curves]            |             |             | ✔        | ✔          |               |                |             |               |
| `'HDKF'`                             |             |             |          |            | ✔             | ✔              |             |               |
| `'HMAC'`                             |             |             | ✔        | ✔          |               |                |             |               |
| `'ML-DSA-44'`[^modern-algos]         |             |             | ✔        | ✔          |               |                |             |               |
| `'ML-DSA-65'`[^modern-algos]         |             |             | ✔        | ✔          |               |                |             |               |
| `'ML-DSA-87'`[^modern-algos]         |             |             | ✔        | ✔          |               |                |             |               |
| `'PBKDF2'`                           |             |             |          |            | ✔             | ✔              |             |               |
| `'RSA-OAEP'`                         | ✔           | ✔           |          |            |               |                | ✔           | ✔             |
| `'RSA-PSS'`                          |             |             | ✔        | ✔          |               |                |             |               |
| `'RSASSA-PKCS1-v1_5'`                |             |             | ✔        | ✔          |               |                |             |               |
| `'X25519'`                           |             |             |          |            | ✔             | ✔              |             |               |
| `'X448'`[^secure-curves]             |             |             |          |            | ✔             | ✔              |             |               |

## Class: `CryptoKeyPair`

<!-- YAML
added: v15.0.0
-->

The `CryptoKeyPair` is a simple dictionary object with `publicKey` and
`privateKey` properties, representing an asymmetric key pair.

### `cryptoKeyPair.privateKey`

<!-- YAML
added: v15.0.0
-->

* Type: {CryptoKey} A {CryptoKey} whose `type` will be `'private'`.

### `cryptoKeyPair.publicKey`

<!-- YAML
added: v15.0.0
-->

* Type: {CryptoKey} A {CryptoKey} whose `type` will be `'public'`.

## Class: `SubtleCrypto`

<!-- YAML
added: v15.0.0
-->

### Static method: `SubtleCrypto.supports(operation, algorithm[, lengthOrAdditionalAlgorithm])`

<!-- YAML
added: REPLACEME
-->

> Stability: 1.1 - Active development

<!--lint disable maximum-line-length remark-lint-->

* `operation` {string} "encrypt", "decrypt", "sign", "verify", "digest", "generateKey", "deriveKey", "deriveBits", "importKey", "exportKey", "getPublicKey", "wrapKey", or "unwrapKey"
* `algorithm` {string|Algorithm}
* `lengthOrAdditionalAlgorithm` {null|number|string|Algorithm|undefined} Depending on the operation this is either ignored, the value of the length argument when operation is "deriveBits", the algorithm of key to be derived when operation is "deriveKey", the algorithm of key to be exported before wrapping when operation is "wrapKey", or the algorithm of key to be imported after unwrapping when operation is "unwrapKey". **Default:** `null` when operation is "deriveBits", `undefined` otherwise.
* Returns: {boolean} Indicating whether the implementation supports the given operation

<!--lint enable maximum-line-length remark-lint-->

Allows feature detection in Web Crypto API,
which can be used to detect whether a given algorithm identifier
(including its parameters) is supported for the given operation.

### `subtle.decrypt(algorithm, key, data)`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ChaCha20-Poly1305 algorithm is now supported.
-->

* `algorithm` {RsaOaepParams|AesCtrParams|AesCbcParams|AeadParams}
* `key` {CryptoKey}
* `data` {ArrayBuffer|TypedArray|DataView|Buffer}
* Returns: {Promise} Fulfills with an {ArrayBuffer} upon success.

Using the method and parameters specified in `algorithm` and the keying
material provided by `key`, `subtle.decrypt()` attempts to decipher the
provided `data`. If successful, the returned promise will be resolved with
an {ArrayBuffer} containing the plaintext result.

The algorithms currently supported include:

* `'AES-CBC'`
* `'AES-CTR'`
* `'AES-GCM'`
* `'ChaCha20-Poly1305'`[^modern-algos]
* `'RSA-OAEP'`

### `subtle.deriveBits(algorithm, baseKey[, length])`

<!-- YAML
added: v15.0.0
changes:
  - version:
    - v22.5.0
    - v20.17.0
    - v18.20.5
    pr-url: https://github.com/nodejs/node/pull/53601
    description: The length parameter is now optional for `'ECDH'`, `'X25519'`,
                 and `'X448'`.
  - version:
    - v18.4.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42507
    description: Added `'X25519'`, and `'X448'` algorithms.
-->

<!--lint disable maximum-line-length remark-lint-->

* `algorithm` {EcdhKeyDeriveParams|HkdfParams|Pbkdf2Params}
* `baseKey` {CryptoKey}
* `length` {number|null} **Default:** `null`
* Returns: {Promise} Fulfills with an {ArrayBuffer} upon success.

<!--lint enable maximum-line-length remark-lint-->

Using the method and parameters specified in `algorithm` and the keying
material provided by `baseKey`, `subtle.deriveBits()` attempts to generate
`length` bits.

When `length` is not provided or `null` the maximum number of bits for a given
algorithm is generated. This is allowed for the `'ECDH'`, `'X25519'`, and `'X448'`[^secure-curves]
algorithms, for other algorithms `length` is required to be a number.

If successful, the returned promise will be resolved with an {ArrayBuffer}
containing the generated data.

The algorithms currently supported include:

* `'ECDH'`
* `'HKDF'`
* `'PBKDF2'`
* `'X25519'`
* `'X448'`[^secure-curves]

### `subtle.deriveKey(algorithm, baseKey, derivedKeyAlgorithm, extractable, keyUsages)`

<!-- YAML
added: v15.0.0
changes:
  - version:
    - v18.4.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42507
    description: Added `'X25519'`, and `'X448'` algorithms.
-->

<!--lint disable maximum-line-length remark-lint-->

* `algorithm` {EcdhKeyDeriveParams|HkdfParams|Pbkdf2Params}
* `baseKey` {CryptoKey}
* `derivedKeyAlgorithm` {string|Algorithm|HmacImportParams|AesDerivedKeyParams}
* `extractable` {boolean}
* `keyUsages` {string\[]} See [Key usages][].
* Returns: {Promise} Fulfills with a {CryptoKey} upon success.

<!--lint enable maximum-line-length remark-lint-->

Using the method and parameters specified in `algorithm`, and the keying
material provided by `baseKey`, `subtle.deriveKey()` attempts to generate
a new {CryptoKey} based on the method and parameters in `derivedKeyAlgorithm`.

Calling `subtle.deriveKey()` is equivalent to calling `subtle.deriveBits()` to
generate raw keying material, then passing the result into the
`subtle.importKey()` method using the `deriveKeyAlgorithm`, `extractable`, and
`keyUsages` parameters as input.

The algorithms currently supported include:

* `'ECDH'`
* `'HKDF'`
* `'PBKDF2'`
* `'X25519'`
* `'X448'`[^secure-curves]

### `subtle.digest(algorithm, data)`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: SHA-3 algorithms are now supported.
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: SHAKE algorithms are now supported.
-->

* `algorithm` {string|Algorithm|CShakeParams}
* `data` {ArrayBuffer|TypedArray|DataView|Buffer}
* Returns: {Promise} Fulfills with an {ArrayBuffer} upon success.

Using the method identified by `algorithm`, `subtle.digest()` attempts to
generate a digest of `data`. If successful, the returned promise is resolved
with an {ArrayBuffer} containing the computed digest.

If `algorithm` is provided as a {string}, it must be one of:

* `'cSHAKE128'`[^modern-algos]
* `'cSHAKE256'`[^modern-algos]
* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`
* `'SHA3-256'`[^modern-algos]
* `'SHA3-384'`[^modern-algos]
* `'SHA3-512'`[^modern-algos]

If `algorithm` is provided as an {Object}, it must have a `name` property
whose value is one of the above.

### `subtle.encrypt(algorithm, key, data)`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ChaCha20-Poly1305 algorithm is now supported.
-->

* `algorithm` {RsaOaepParams|AesCtrParams|AesCbcParams|AeadParams}
* `key` {CryptoKey}
* `data` {ArrayBuffer|TypedArray|DataView|Buffer}
* Returns: {Promise} Fulfills with an {ArrayBuffer} upon success.

Using the method and parameters specified by `algorithm` and the keying
material provided by `key`, `subtle.encrypt()` attempts to encipher `data`.
If successful, the returned promise is resolved with an {ArrayBuffer}
containing the encrypted result.

The algorithms currently supported include:

* `'AES-CBC'`
* `'AES-CTR'`
* `'AES-GCM'`
* `'ChaCha20-Poly1305'`[^modern-algos]
* `'RSA-OAEP'`

### `subtle.exportKey(format, key)`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ChaCha20-Poly1305 algorithm is now supported.
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ML-DSA algorithms are now supported.
  - version:
    - v18.4.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42507
    description: Added `'Ed25519'`, `'Ed448'`, `'X25519'`, and `'X448'`
      algorithms.
  - version: v15.9.0
    pr-url: https://github.com/nodejs/node/pull/37203
    description: Removed `'NODE-DSA'` JWK export.
-->

* `format` {string} Must be one of `'raw'`, `'pkcs8'`, `'spki'`, `'jwk'`, `'raw-secret'`[^modern-algos],
  `'raw-public'`[^modern-algos], or `'raw-seed'`[^modern-algos].
* `key` {CryptoKey}
* Returns: {Promise} Fulfills with an {ArrayBuffer|Object} upon success.

Exports the given key into the specified format, if supported.

If the {CryptoKey} is not extractable, the returned promise will reject.

When `format` is either `'pkcs8'` or `'spki'` and the export is successful,
the returned promise will be resolved with an {ArrayBuffer} containing the
exported key data.

When `format` is `'jwk'` and the export is successful, the returned promise
will be resolved with a JavaScript object conforming to the [JSON Web Key][]
specification.

| Supported Key Algorithm              | `'spki'` | `'pkcs8'` | `'jwk'` | `'raw'` | `'raw-secret'` | `'raw-public'` | `'raw-seed'` |
| ------------------------------------ | -------- | --------- | ------- | ------- | -------------- | -------------- | ------------ |
| `'AES-CBC'`                          |          |           | ✔       | ✔       | ✔              |                |              |
| `'AES-CTR'`                          |          |           | ✔       | ✔       | ✔              |                |              |
| `'AES-GCM'`                          |          |           | ✔       | ✔       | ✔              |                |              |
| `'AES-KW'`                           |          |           | ✔       | ✔       | ✔              |                |              |
| `'ChaCha20-Poly1305'`[^modern-algos] |          |           | ✔       |         | ✔              |                |              |
| `'ECDH'`                             | ✔        | ✔         | ✔       | ✔       |                | ✔              |              |
| `'ECDSA'`                            | ✔        | ✔         | ✔       | ✔       |                | ✔              |              |
| `'Ed25519'`                          | ✔        | ✔         | ✔       | ✔       |                | ✔              |              |
| `'Ed448'`[^secure-curves]            | ✔        | ✔         | ✔       | ✔       |                | ✔              |              |
| `'HMAC'`                             |          |           | ✔       | ✔       | ✔              |                |              |
| `'ML-DSA-44'`[^modern-algos]         | ✔        | ✔         | ✔       |         |                | ✔              | ✔            |
| `'ML-DSA-65'`[^modern-algos]         | ✔        | ✔         | ✔       |         |                | ✔              | ✔            |
| `'ML-DSA-87'`[^modern-algos]         | ✔        | ✔         | ✔       |         |                | ✔              | ✔            |
| `'RSA-OAEP'`                         | ✔        | ✔         | ✔       |         |                |                |              |
| `'RSA-PSS'`                          | ✔        | ✔         | ✔       |         |                |                |              |
| `'RSASSA-PKCS1-v1_5'`                | ✔        | ✔         | ✔       |         |                |                |              |

### `subtle.getPublicKey(key, keyUsages)`

<!-- YAML
added: REPLACEME
-->

> Stability: 1.1 - Active development

* `key` {CryptoKey} A private key from which to derive the corresponding public key.
* `keyUsages` {string\[]} See [Key usages][].
* Returns: {Promise} Fulfills with a {CryptoKey} upon success.

Derives the public key from a given private key.

### `subtle.generateKey(algorithm, extractable, keyUsages)`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ChaCha20-Poly1305 algorithm is now supported.
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ML-DSA algorithms are now supported.
-->

<!--lint disable maximum-line-length remark-lint-->

* `algorithm` {string|Algorithm|RsaHashedKeyGenParams|EcKeyGenParams|HmacKeyGenParams|AesKeyGenParams}

<!--lint enable maximum-line-length remark-lint-->

* `extractable` {boolean}
* `keyUsages` {string\[]} See [Key usages][].
* Returns: {Promise} Fulfills with a {CryptoKey|CryptoKeyPair} upon success.

Using the method and parameters provided in `algorithm`, `subtle.generateKey()`
attempts to generate new keying material. Depending the method used, the method
may generate either a single {CryptoKey} or a {CryptoKeyPair}.

The {CryptoKeyPair} (public and private key) generating algorithms supported
include:

* `'ECDH'`
* `'ECDSA'`
* `'Ed25519'`
* `'Ed448'`[^secure-curves]
* `'ML-DSA-44'`[^modern-algos]
* `'ML-DSA-65'`[^modern-algos]
* `'ML-DSA-87'`[^modern-algos]
* `'RSA-OAEP'`
* `'RSA-PSS'`
* `'RSASSA-PKCS1-v1_5'`
* `'X25519'`
* `'X448'`[^secure-curves]

The {CryptoKey} (secret key) generating algorithms supported include:

* `'AES-CBC'`
* `'AES-CTR'`
* `'AES-GCM'`
* `'AES-KW'`
* `'ChaCha20-Poly1305'`[^modern-algos]
* `'HMAC'`

### `subtle.importKey(format, keyData, algorithm, extractable, keyUsages)`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ChaCha20-Poly1305 algorithm is now supported.
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ML-DSA algorithms are now supported.
  - version:
    - v18.4.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42507
    description: Added `'Ed25519'`, `'Ed448'`, `'X25519'`, and `'X448'`
      algorithms.
  - version: v15.9.0
    pr-url: https://github.com/nodejs/node/pull/37203
    description: Removed `'NODE-DSA'` JWK import.
-->

* `format` {string} Must be one of `'raw'`, `'pkcs8'`, `'spki'`, `'jwk'`, `'raw-secret'`[^modern-algos],
  `'raw-public'`[^modern-algos], or `'raw-seed'`[^modern-algos].
* `keyData` {ArrayBuffer|TypedArray|DataView|Buffer|Object}

<!--lint disable maximum-line-length remark-lint-->

* `algorithm` {string|Algorithm|RsaHashedImportParams|EcKeyImportParams|HmacImportParams}

<!--lint enable maximum-line-length remark-lint-->

* `extractable` {boolean}
* `keyUsages` {string\[]} See [Key usages][].
* Returns: {Promise} Fulfills with a {CryptoKey} upon success.

The `subtle.importKey()` method attempts to interpret the provided `keyData`
as the given `format` to create a {CryptoKey} instance using the provided
`algorithm`, `extractable`, and `keyUsages` arguments. If the import is
successful, the returned promise will be resolved with the created {CryptoKey}.

If importing a `'PBKDF2'` key, `extractable` must be `false`.

The algorithms currently supported include:

| Supported Key Algorithm              | `'spki'` | `'pkcs8'` | `'jwk'` | `'raw'` | `'raw-secret'` | `'raw-public'` | `'raw-seed'` |
| ------------------------------------ | -------- | --------- | ------- | ------- | -------------- | -------------- | ------------ |
| `'AES-CBC'`                          |          |           | ✔       | ✔       | ✔              |                |              |
| `'AES-CTR'`                          |          |           | ✔       | ✔       | ✔              |                |              |
| `'AES-GCM'`                          |          |           | ✔       | ✔       | ✔              |                |              |
| `'AES-KW'`                           |          |           | ✔       | ✔       | ✔              |                |              |
| `'ChaCha20-Poly1305'`[^modern-algos] |          |           | ✔       |         | ✔              |                |              |
| `'ECDH'`                             | ✔        | ✔         | ✔       | ✔       |                | ✔              |              |
| `'ECDSA'`                            | ✔        | ✔         | ✔       | ✔       |                | ✔              |              |
| `'Ed25519'`                          | ✔        | ✔         | ✔       | ✔       |                | ✔              |              |
| `'Ed448'`[^secure-curves]            | ✔        | ✔         | ✔       | ✔       |                | ✔              |              |
| `'HDKF'`                             |          |           |         | ✔       | ✔              |                |              |
| `'HMAC'`                             |          |           | ✔       | ✔       | ✔              |                |              |
| `'ML-DSA-44'`[^modern-algos]         | ✔        | ✔         | ✔       |         |                | ✔              | ✔            |
| `'ML-DSA-65'`[^modern-algos]         | ✔        | ✔         | ✔       |         |                | ✔              | ✔            |
| `'ML-DSA-87'`[^modern-algos]         | ✔        | ✔         | ✔       |         |                | ✔              | ✔            |
| `'PBKDF2'`                           |          |           |         | ✔       | ✔              |                |              |
| `'RSA-OAEP'`                         | ✔        | ✔         | ✔       |         |                |                |              |
| `'RSA-PSS'`                          | ✔        | ✔         | ✔       |         |                |                |              |
| `'RSASSA-PKCS1-v1_5'`                | ✔        | ✔         | ✔       |         |                |                |              |
| `'X25519'`                           | ✔        | ✔         | ✔       | ✔       |                | ✔              |              |
| `'X448'`[^secure-curves]             | ✔        | ✔         | ✔       | ✔       |                | ✔              |              |

### `subtle.sign(algorithm, key, data)`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ML-DSA algorithms are now supported.
  - version:
    - v18.4.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42507
    description: Added `'Ed25519'`, and `'Ed448'` algorithms.
-->

<!--lint disable maximum-line-length remark-lint-->

* `algorithm` {string|Algorithm|RsaPssParams|EcdsaParams|Ed448Params|ContextParams}
* `key` {CryptoKey}
* `data` {ArrayBuffer|TypedArray|DataView|Buffer}
* Returns: {Promise} Fulfills with an {ArrayBuffer} upon success.

<!--lint enable maximum-line-length remark-lint-->

Using the method and parameters given by `algorithm` and the keying material
provided by `key`, `subtle.sign()` attempts to generate a cryptographic
signature of `data`. If successful, the returned promise is resolved with
an {ArrayBuffer} containing the generated signature.

The algorithms currently supported include:

* `'ECDSA'`
* `'Ed25519'`
* `'Ed448'`[^secure-curves]
* `'HMAC'`
* `'ML-DSA-44'`[^modern-algos]
* `'ML-DSA-65'`[^modern-algos]
* `'ML-DSA-87'`[^modern-algos]
* `'RSA-PSS'`
* `'RSASSA-PKCS1-v1_5'`

### `subtle.unwrapKey(format, wrappedKey, unwrappingKey, unwrapAlgo, unwrappedKeyAlgo, extractable, keyUsages)`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ChaCha20-Poly1305 algorithm is now supported.
-->

* `format` {string} Must be one of `'raw'`, `'pkcs8'`, `'spki'`, `'jwk'`, `'raw-secret'`[^modern-algos],
  `'raw-public'`[^modern-algos], or `'raw-seed'`[^modern-algos].
* `wrappedKey` {ArrayBuffer|TypedArray|DataView|Buffer}
* `unwrappingKey` {CryptoKey}

<!--lint disable maximum-line-length remark-lint-->

* `unwrapAlgo` {string|Algorithm|RsaOaepParams|AesCtrParams|AesCbcParams|AeadParams}
* `unwrappedKeyAlgo` {string|Algorithm|RsaHashedImportParams|EcKeyImportParams|HmacImportParams}

<!--lint enable maximum-line-length remark-lint-->

* `extractable` {boolean}
* `keyUsages` {string\[]} See [Key usages][].
* Returns: {Promise} Fulfills with a {CryptoKey} upon success.

In cryptography, "wrapping a key" refers to exporting and then encrypting the
keying material. The `subtle.unwrapKey()` method attempts to decrypt a wrapped
key and create a {CryptoKey} instance. It is equivalent to calling
`subtle.decrypt()` first on the encrypted key data (using the `wrappedKey`,
`unwrapAlgo`, and `unwrappingKey` arguments as input) then passing the results
in to the `subtle.importKey()` method using the `unwrappedKeyAlgo`,
`extractable`, and `keyUsages` arguments as inputs. If successful, the returned
promise is resolved with a {CryptoKey} object.

The wrapping algorithms currently supported include:

* `'AES-CBC'`
* `'AES-CTR'`
* `'AES-GCM'`
* `'AES-KW'`
* `'ChaCha20-Poly1305'`[^modern-algos]
* `'RSA-OAEP'`

The unwrapped key algorithms supported include:

* `'AES-CBC'`
* `'AES-CTR'`
* `'AES-GCM'`
* `'AES-KW'`
* `'ChaCha20-Poly1305'`[^modern-algos]
* `'ECDH'`
* `'ECDSA'`
* `'Ed25519'`
* `'Ed448'`[^secure-curves]
* `'HMAC'`
* `'ML-DSA-44'`[^modern-algos]
* `'ML-DSA-65'`[^modern-algos]
* `'ML-DSA-87'`[^modern-algos]
* `'RSA-OAEP'`
* `'RSA-PSS'`
* `'RSASSA-PKCS1-v1_5'`
* `'X25519'`
* `'X448'`[^secure-curves]

### `subtle.verify(algorithm, key, signature, data)`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ML-DSA algorithms are now supported.
  - version:
    - v18.4.0
    - v16.17.0
    pr-url: https://github.com/nodejs/node/pull/42507
    description: Added `'Ed25519'`, and `'Ed448'` algorithms.
-->

<!--lint disable maximum-line-length remark-lint-->

* `algorithm` {string|Algorithm|RsaPssParams|EcdsaParams|Ed448Params|ContextParams}
* `key` {CryptoKey}
* `signature` {ArrayBuffer|TypedArray|DataView|Buffer}
* `data` {ArrayBuffer|TypedArray|DataView|Buffer}
* Returns: {Promise} Fulfills with a {boolean} upon success.

<!--lint enable maximum-line-length remark-lint-->

Using the method and parameters given in `algorithm` and the keying material
provided by `key`, `subtle.verify()` attempts to verify that `signature` is
a valid cryptographic signature of `data`. The returned promise is resolved
with either `true` or `false`.

The algorithms currently supported include:

* `'ECDSA'`
* `'Ed25519'`
* `'Ed448'`[^secure-curves]
* `'HMAC'`
* `'ML-DSA-44'`[^modern-algos]
* `'ML-DSA-65'`[^modern-algos]
* `'ML-DSA-87'`[^modern-algos]
* `'RSA-PSS'`
* `'RSASSA-PKCS1-v1_5'`

### `subtle.wrapKey(format, key, wrappingKey, wrapAlgo)`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: ChaCha20-Poly1305 algorithm is now supported.
-->

<!--lint disable maximum-line-length remark-lint-->

* `format` {string} Must be one of `'raw'`, `'pkcs8'`, `'spki'`, `'jwk'`, `'raw-secret'`[^modern-algos],
  `'raw-public'`[^modern-algos], or `'raw-seed'`[^modern-algos].
* `key` {CryptoKey}
* `wrappingKey` {CryptoKey}
* `wrapAlgo` {string|Algorithm|RsaOaepParams|AesCtrParams|AesCbcParams|AeadParams}
* Returns: {Promise} Fulfills with an {ArrayBuffer} upon success.

<!--lint enable maximum-line-length remark-lint-->

In cryptography, "wrapping a key" refers to exporting and then encrypting the
keying material. The `subtle.wrapKey()` method exports the keying material into
the format identified by `format`, then encrypts it using the method and
parameters specified by `wrapAlgo` and the keying material provided by
`wrappingKey`. It is the equivalent to calling `subtle.exportKey()` using
`format` and `key` as the arguments, then passing the result to the
`subtle.encrypt()` method using `wrappingKey` and `wrapAlgo` as inputs. If
successful, the returned promise will be resolved with an {ArrayBuffer}
containing the encrypted key data.

The wrapping algorithms currently supported include:

* `'AES-CBC'`
* `'AES-CTR'`
* `'AES-GCM'`
* `'AES-KW'`
* `'ChaCha20-Poly1305'`[^modern-algos]
* `'RSA-OAEP'`

## Algorithm parameters

The algorithm parameter objects define the methods and parameters used by
the various {SubtleCrypto} methods. While described here as "classes", they
are simple JavaScript dictionary objects.

### Class: `Algorithm`

<!-- YAML
added: v15.0.0
-->

#### `Algorithm.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string}

### Class: `AeadParams`

<!-- YAML
added: v15.0.0
-->

#### `aeadParams.additionalData`

<!-- YAML
added: v15.0.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer|undefined}

Extra input that is not encrypted but is included in the authentication
of the data. The use of `additionalData` is optional.

#### `aeadParams.iv`

<!-- YAML
added: v15.0.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer}

The initialization vector must be unique for every encryption operation using a
given key.

#### `aeadParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'AES-GCM'` or `'ChaCha20-Poly1305'`.

#### `aeadParams.tagLength`

<!-- YAML
added: v15.0.0
-->

* Type: {number} The size in bits of the generated authentication tag.

### Class: `AesDerivedKeyParams`

<!-- YAML
added: v15.0.0
-->

#### `aesDerivedKeyParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be one of `'AES-CBC'`, `'AES-CTR'`, `'AES-GCM'`, or
  `'AES-KW'`

#### `aesDerivedKeyParams.length`

<!-- YAML
added: v15.0.0
-->

* Type: {number}

The length of the AES key to be derived. This must be either `128`, `192`,
or `256`.

### Class: `AesCbcParams`

<!-- YAML
added: v15.0.0
-->

#### `aesCbcParams.iv`

<!-- YAML
added: v15.0.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer}

Provides the initialization vector. It must be exactly 16-bytes in length
and should be unpredictable and cryptographically random.

#### `aesCbcParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'AES-CBC'`.

### Class: `AesCtrParams`

<!-- YAML
added: v15.0.0
-->

#### `aesCtrParams.counter`

<!-- YAML
added: v15.0.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer}

The initial value of the counter block. This must be exactly 16 bytes long.

The `AES-CTR` method uses the rightmost `length` bits of the block as the
counter and the remaining bits as the nonce.

#### `aesCtrParams.length`

<!-- YAML
added: v15.0.0
-->

* Type: {number} The number of bits in the `aesCtrParams.counter` that are
  to be used as the counter.

#### `aesCtrParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'AES-CTR'`.

### Class: `AesKeyAlgorithm`

<!-- YAML
added: v15.0.0
-->

#### `aesKeyAlgorithm.length`

<!-- YAML
added: v15.0.0
-->

* Type: {number}

The length of the AES key in bits.

#### `aesKeyAlgorithm.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string}

### Class: `AesKeyGenParams`

<!-- YAML
added: v15.0.0
-->

#### `aesKeyGenParams.length`

<!-- YAML
added: v15.0.0
-->

* Type: {number}

The length of the AES key to be generated. This must be either `128`, `192`,
or `256`.

#### `aesKeyGenParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be one of `'AES-CBC'`, `'AES-CTR'`, `'AES-GCM'`, or
  `'AES-KW'`

### Class: `ContextParams`

<!-- YAML
added: REPLACEME
-->

#### `contextParams.name`

<!-- YAML
added: REPLACEME
-->

* Type: {string} Must be `'ML-DSA-44'`[^modern-algos], `'ML-DSA-65'`[^modern-algos], or `'ML-DSA-87'`[^modern-algos].

#### `contextParams.context`

<!-- YAML
added: REPLACEME
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer|undefined}

The `context` member represents the optional context data to associate with
the message.
The Node.js Web Crypto API implementation only supports zero-length context
which is equivalent to not providing context at all.

### Class: `CShakeParams`

<!-- YAML
added: REPLACEME
-->

#### `cShakeParams.customization`

<!-- YAML
added: REPLACEME
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer|undefined}

The `customization` member represents the customization string.
The Node.js Web Crypto API implementation only supports zero-length customization
which is equivalent to not providing customization at all.

#### `cShakeParams.functionName`

<!-- YAML
added: REPLACEME
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer|undefined}

The `functionName` member represents represents the function name, used by NIST to define
functions based on cSHAKE.
The Node.js Web Crypto API implementation only supports zero-length functionName
which is equivalent to not providing functionName at all.

#### `cShakeParams.length`

<!-- YAML
added: REPLACEME
-->

* Type: {number} represents the requested output length in bits.

#### `cShakeParams.name`

<!-- YAML
added: REPLACEME
-->

* Type: {string} Must be `'cSHAKE128'`[^modern-algos] or `'cSHAKE256'`[^modern-algos]

### Class: `EcdhKeyDeriveParams`

<!-- YAML
added: v15.0.0
-->

#### `ecdhKeyDeriveParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'ECDH'`, `'X25519'`, or `'X448'`[^secure-curves].

#### `ecdhKeyDeriveParams.public`

<!-- YAML
added: v15.0.0
-->

* Type: {CryptoKey}

ECDH key derivation operates by taking as input one parties private key and
another parties public key -- using both to generate a common shared secret.
The `ecdhKeyDeriveParams.public` property is set to the other parties public
key.

### Class: `EcdsaParams`

<!-- YAML
added: v15.0.0
-->

#### `ecdsaParams.hash`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: SHA-3 algorithms are now supported.
-->

* Type: {string|Algorithm}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`
* `'SHA3-256'`[^modern-algos]
* `'SHA3-384'`[^modern-algos]
* `'SHA3-512'`[^modern-algos]

If represented as an {Algorithm}, the object's `name` property
must be one of the above listed values.

#### `ecdsaParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'ECDSA'`.

### Class: `EcKeyAlgorithm`

<!-- YAML
added: v15.0.0
-->

#### `ecKeyAlgorithm.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string}

#### `ecKeyAlgorithm.namedCurve`

<!-- YAML
added: v15.0.0
-->

* Type: {string}

### Class: `EcKeyGenParams`

<!-- YAML
added: v15.0.0
-->

#### `ecKeyGenParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be one of `'ECDSA'` or `'ECDH'`.

#### `ecKeyGenParams.namedCurve`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be one of `'P-256'`, `'P-384'`, `'P-521'`.

### Class: `EcKeyImportParams`

<!-- YAML
added: v15.0.0
-->

#### `ecKeyImportParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be one of `'ECDSA'` or `'ECDH'`.

#### `ecKeyImportParams.namedCurve`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be one of `'P-256'`, `'P-384'`, `'P-521'`.

### Class: `Ed448Params`

<!-- YAML
added: v15.0.0
-->

#### `ed448Params.name`

<!-- YAML
added:
  - v18.4.0
  - v16.17.0
-->

* Type: {string} Must be `'Ed448'`[^secure-curves].

#### `ed448Params.context`

<!-- YAML
added:
  - v18.4.0
  - v16.17.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer|undefined}

The `context` member represents the optional context data to associate with
the message.
The Node.js Web Crypto API implementation only supports zero-length context
which is equivalent to not providing context at all.

### Class: `HkdfParams`

<!-- YAML
added: v15.0.0
-->

#### `hkdfParams.hash`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: SHA-3 algorithms are now supported.
-->

* Type: {string|Algorithm}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`
* `'SHA3-256'`[^modern-algos]
* `'SHA3-384'`[^modern-algos]
* `'SHA3-512'`[^modern-algos]

If represented as an {Algorithm}, the object's `name` property
must be one of the above listed values.

#### `hkdfParams.info`

<!-- YAML
added: v15.0.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer}

Provides application-specific contextual input to the HKDF algorithm.
This can be zero-length but must be provided.

#### `hkdfParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'HKDF'`.

#### `hkdfParams.salt`

<!-- YAML
added: v15.0.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer}

The salt value significantly improves the strength of the HKDF algorithm.
It should be random or pseudorandom and should be the same length as the
output of the digest function (for instance, if using `'SHA-256'` as the
digest, the salt should be 256-bits of random data).

### Class: `HmacImportParams`

<!-- YAML
added: v15.0.0
-->

#### `hmacImportParams.hash`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: SHA-3 algorithms are now supported.
-->

* Type: {string|Algorithm}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`
* `'SHA3-256'`[^modern-algos]
* `'SHA3-384'`[^modern-algos]
* `'SHA3-512'`[^modern-algos]

If represented as an {Algorithm}, the object's `name` property
must be one of the above listed values.

#### `hmacImportParams.length`

<!-- YAML
added: v15.0.0
-->

* Type: {number}

The optional number of bits in the HMAC key. This is optional and should
be omitted for most cases.

#### `hmacImportParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'HMAC'`.

### Class: `HmacKeyAlgorithm`

<!-- YAML
added: v15.0.0
-->

#### `hmacKeyAlgorithm.hash`

<!-- YAML
added: v15.0.0
-->

* Type: {Algorithm}

#### `hmacKeyAlgorithm.length`

<!-- YAML
added: v15.0.0
-->

* Type: {number}

The length of the HMAC key in bits.

#### `hmacKeyAlgorithm.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string}

### Class: `HmacKeyGenParams`

<!-- YAML
added: v15.0.0
-->

#### `hmacKeyGenParams.hash`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: SHA-3 algorithms are now supported.
-->

* Type: {string|Algorithm}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`
* `'SHA3-256'`[^modern-algos]
* `'SHA3-384'`[^modern-algos]
* `'SHA3-512'`[^modern-algos]

If represented as an {Algorithm}, the object's `name` property
must be one of the above listed values.

#### `hmacKeyGenParams.length`

<!-- YAML
added: v15.0.0
-->

* Type: {number}

The number of bits to generate for the HMAC key. If omitted,
the length will be determined by the hash algorithm used.
This is optional and should be omitted for most cases.

#### `hmacKeyGenParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'HMAC'`.

### Class: `KeyAlgorithm`

<!-- YAML
added: v15.0.0
-->

#### `keyAlgorithm.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string}

### Class: `Pbkdf2Params`

<!-- YAML
added: v15.0.0
-->

#### `pbkdf2Params.hash`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: SHA-3 algorithms are now supported.
-->

* Type: {string|Algorithm}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`
* `'SHA3-256'`[^modern-algos]
* `'SHA3-384'`[^modern-algos]
* `'SHA3-512'`[^modern-algos]

If represented as an {Algorithm}, the object's `name` property
must be one of the above listed values.

#### `pbkdf2Params.iterations`

<!-- YAML
added: v15.0.0
-->

* Type: {number}

The number of iterations the PBKDF2 algorithm should make when deriving bits.

#### `pbkdf2Params.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'PBKDF2'`.

#### `pbkdf2Params.salt`

<!-- YAML
added: v15.0.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer}

Should be at least 16 random or pseudorandom bytes.

### Class: `RsaHashedImportParams`

<!-- YAML
added: v15.0.0
-->

#### `rsaHashedImportParams.hash`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: SHA-3 algorithms are now supported.
-->

* Type: {string|Algorithm}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`
* `'SHA3-256'`[^modern-algos]
* `'SHA3-384'`[^modern-algos]
* `'SHA3-512'`[^modern-algos]

If represented as an {Algorithm}, the object's `name` property
must be one of the above listed values.

#### `rsaHashedImportParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be one of `'RSASSA-PKCS1-v1_5'`, `'RSA-PSS'`, or
  `'RSA-OAEP'`.

### Class: `RsaHashedKeyAlgorithm`

<!-- YAML
added: v15.0.0
-->

#### `rsaHashedKeyAlgorithm.hash`

<!-- YAML
added: v15.0.0
-->

* Type: {Algorithm}

#### `rsaHashedKeyAlgorithm.modulusLength`

<!-- YAML
added: v15.0.0
-->

* Type: {number}

The length in bits of the RSA modulus.

#### `rsaHashedKeyAlgorithm.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string}

#### `rsaHashedKeyAlgorithm.publicExponent`

<!-- YAML
added: v15.0.0
-->

* Type: {Uint8Array}

The RSA public exponent.

### Class: `RsaHashedKeyGenParams`

<!-- YAML
added: v15.0.0
-->

#### `rsaHashedKeyGenParams.hash`

<!-- YAML
added: v15.0.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/59365
    description: SHA-3 algorithms are now supported.
-->

* Type: {string|Algorithm}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`
* `'SHA3-256'`[^modern-algos]
* `'SHA3-384'`[^modern-algos]
* `'SHA3-512'`[^modern-algos]

If represented as an {Algorithm}, the object's `name` property
must be one of the above listed values.

#### `rsaHashedKeyGenParams.modulusLength`

<!-- YAML
added: v15.0.0
-->

* Type: {number}

The length in bits of the RSA modulus. As a best practice, this should be
at least `2048`.

#### `rsaHashedKeyGenParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be one of `'RSASSA-PKCS1-v1_5'`, `'RSA-PSS'`, or
  `'RSA-OAEP'`.

#### `rsaHashedKeyGenParams.publicExponent`

<!-- YAML
added: v15.0.0
-->

* Type: {Uint8Array}

The RSA public exponent. This must be a {Uint8Array} containing a big-endian,
unsigned integer that must fit within 32-bits. The {Uint8Array} may contain an
arbitrary number of leading zero-bits. The value must be a prime number. Unless
there is reason to use a different value, use `new Uint8Array([1, 0, 1])`
(65537) as the public exponent.

### Class: `RsaOaepParams`

<!-- YAML
added: v15.0.0
-->

#### `rsaOaepParams.label`

<!-- YAML
added: v15.0.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer}

An additional collection of bytes that will not be encrypted, but will be bound
to the generated ciphertext.

The `rsaOaepParams.label` parameter is optional.

#### `rsaOaepParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} must be `'RSA-OAEP'`.

### Class: `RsaPssParams`

<!-- YAML
added: v15.0.0
-->

#### `rsaPssParams.name`

<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'RSA-PSS'`.

#### `rsaPssParams.saltLength`

<!-- YAML
added: v15.0.0
-->

* Type: {number}

The length (in bytes) of the random salt to use.

[^secure-curves]: See [Secure Curves in the Web Cryptography API][]

[^modern-algos]: See [Modern Algorithms in the Web Cryptography API][]

[^openssl35]: Requires OpenSSL >= 3.5

[JSON Web Key]: https://tools.ietf.org/html/rfc7517
[Key usages]: #cryptokeyusages
[Modern Algorithms in the Web Cryptography API]: #modern-algorithms-in-the-web-cryptography-api
[RFC 4122]: https://www.rfc-editor.org/rfc/rfc4122.txt
[Secure Curves in the Web Cryptography API]: #secure-curves-in-the-web-cryptography-api
[Web Crypto API]: https://www.w3.org/TR/WebCryptoAPI/
[`SubtleCrypto.supports()`]: #static-method-subtlecryptosupportsoperation-algorithm-lengthoradditionalalgorithm
[`subtle.getPublicKey()`]: #subtlegetpublickeykey-keyusages
