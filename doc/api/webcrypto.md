# Web Crypto API

<!-- introduced_in=v15.0.0 -->

> Stability: 1 - Experimental

Node.js provides an implementation of the standard [Web Crypto API][].

Use `require('crypto').webcrypto` to access this module.

```js
const { subtle } = require('crypto').webcrypto;

(async function() {

  const key = await subtle.generateKey({
    name: 'HMAC',
    hash: 'SHA-256',
    length: 256
  }, true, ['sign', 'verify']);

  const digest = await subtle.sign({
    name: 'HMAC'
  }, key, 'I love cupcakes');

})();
```

## Examples

### Generating keys

The {SubtleCrypto} class can be used to generate symmetric (secret) keys
or asymmetric key pairs (public key and private key).

#### AES keys

```js
const { subtle } = require('crypto').webcrypto;

async function generateAesKey(length = 256) {
  const key = await subtle.generateKey({
    name: 'AES-CBC',
    length
  }, true, ['encrypt', 'decrypt']);

  return key;
}
```

#### Elliptic curve key pairs

```js
const { subtle } = require('crypto').webcrypto;

async function generateEcKey(namedCurve = 'P-521') {
  const {
    publicKey,
    privateKey
  } = await subtle.generateKey({
    name: 'ECDSA',
    namedCurve,
  }, true, ['sign', 'verify']);

  return { publicKey, privateKey };
}
```

#### ED25519/ED448/X25519/X448 Elliptic curve key pairs

```js
const { subtle } = require('crypto').webcrypto;

async function generateEd25519Key() {
  return subtle.generateKey({
    name: 'NODE-ED25519',
    namedCurve: 'NODE-ED25519',
  }, true, ['sign', 'verify']);
}

async function generateX25519Key() {
  return subtle.generateKey({
    name: 'ECDH',
    namedCurve: 'NODE-X25519',
  }, true, ['deriveKey']);
}
```

#### HMAC keys

```js
const { subtle } = require('crypto').webcrypto;

async function generateHmacKey(hash = 'SHA-256') {
  const key = await subtle.generateKey({
    name: 'HMAC',
    hash
  }, true, ['sign', 'verify']);

  return key;
}
```

#### RSA key pairs

```js
const { subtle } = require('crypto').webcrypto;
const publicExponent = new Uint8Array([1, 0, 1]);

async function generateRsaKey(modulusLength = 2048, hash = 'SHA-256') {
  const {
    publicKey,
    privateKey
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
const { subtle, getRandomValues } = require('crypto').webcrypto;

async function aesEncrypt(plaintext) {
  const ec = new TextEncoder();
  const key = await generateAesKey();
  const iv = getRandomValues(new Uint8Array(16));

  const ciphertext = await subtle.encrypt({
    name: 'AES-CBC',
    iv,
  }, key, ec.encode(plaintext));

  return {
    key,
    iv,
    ciphertext
  };
}

async function aesDecrypt(ciphertext, key, iv) {
  const dec = new TextDecoder();
  const plaintext = await subtle.decrypt({
    name: 'AES-CBC',
    iv,
  }, key, ciphertext);

  return dec.decode(plaintext);
}
```

### Exporting and importing keys

```js
const { subtle } = require('crypto').webcrypto;

async function generateAndExportHmacKey(format = 'jwk', hash = 'SHA-512') {
  const key = await subtle.generateKey({
    name: 'HMAC',
    hash
  }, true, ['sign', 'verify']);

  return subtle.exportKey(format, key);
}

async function importHmacKey(keyData, format = 'jwk', hash = 'SHA-512') {
  const key = await subtle.importKey(format, keyData, {
    name: 'HMAC',
    hash
  }, true, ['sign', 'verify']);

  return key;
}
```

### Wrapping and unwrapping keys

```js
const { subtle } = require('crypto').webcrypto;

async function generateAndWrapHmacKey(format = 'jwk', hash = 'SHA-512') {
  const [
    key,
    wrappingKey,
  ] = await Promise.all([
    subtle.generateKey({
      name: 'HMAC', hash
    }, true, ['sign', 'verify']),
    subtle.generateKey({
      name: 'AES-KW',
      length: 256
    }, true, ['wrapKey', 'unwrapKey']),
  ]);

  const wrappedKey = await subtle.wrapKey(format, key, wrappingKey, 'AES-KW');

  return wrappedKey;
}

async function unwrapHmacKey(
  wrappedKey,
  wrappingKey,
  format = 'jwk',
  hash = 'SHA-512') {

  const key = await subtle.unwrapKey(
    format,
    wrappedKey,
    unwrappingKey,
    'AES-KW',
    { name: 'HMAC', hash },
    true,
    ['sign', 'verify']);

  return key;
}
```

### Sign and verify

```js
const { subtle } = require('crypto').webcrypto;

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
const { subtle } = require('crypto').webcrypto;

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
    iterations
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
    iterations
  }, keyMaterial, {
    name: 'AES-GCM',
    length: 256
  }, true, ['encrypt', 'decrypt']);
  return key;
}
```

### Digest

```js
const { subtle } = require('crypto').webcrypto;

async function digest(data, algorithm = 'SHA-512') {
  const ec = new TextEncoder();
  const digest = await subtle.digest(algorithm, ec.encode(data));
  return digest;
}
```

## Algorithm Matrix

The table details the algorithms supported by the Node.js Web Crypto API
implementation and the APIs supported for each:

| Algorithm             | `generateKey` | `exportKey` | `importKey` | `encrypt` | `decrypt` | `wrapKey` | `unwrapKey` | `deriveBits` | `deriveKey` | `sign` | `verify` | `digest` |
| --------------------- | ------------- | ----------- | ----------- | --------- | --------- | --------- | ----------- | ------------ | ----------- | ------ | -------- | -------- |
| `'RSASSA-PKCS1-v1_5'` | ✔ | ✔ | ✔ |   |   |   |   |   |   | ✔ | ✔ |   |
| `'RSA-PSS'`           | ✔ | ✔ | ✔ |   |   |   |   |   |   | ✔ | ✔ |   |
| `'RSA-OAEP'`          | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |   |   |   |   |   |
| `'ECDSA'`             | ✔ | ✔ | ✔ |   |   |   |   |   |   | ✔ | ✔ |   |
| `'ECDH'`              | ✔ | ✔ | ✔ |   |   |   |   | ✔ | ✔ |   |   |   |
| `'AES-CTR'`           | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |   |   |   |   |   |
| `'AES-CBC'`           | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |   |   |   |   |   |
| `'AES-GCM'`           | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |   |   |   |   |   |
| `'AES-KW'`            | ✔ | ✔ | ✔ |   |   | ✔ | ✔ |   |   |   |   |   |
| `'HMAC'`              | ✔ | ✔ | ✔ |   |   |   |   |   |   | ✔ | ✔ |   |
| `'HKDF'`              |   | ✔ | ✔ |   |   |   |   | ✔ | ✔ |   |   |   |
| `'PBKDF2'`            |   | ✔ | ✔ |   |   |   |   | ✔ | ✔ |   |   |   |
| `'SHA-1'`             |   |   |   |   |   |   |   |   |   |   |   | ✔ |
| `'SHA-256'`           |   |   |   |   |   |   |   |   |   |   |   | ✔ |
| `'SHA-384'`           |   |   |   |   |   |   |   |   |   |   |   | ✔ |
| `'SHA-512'`           |   |   |   |   |   |   |   |   |   |   |   | ✔ |
| `'NODE-DSA'`<sup>1</sup> | ✔ | ✔ | ✔ |   |   |   |   |   |   | ✔ | ✔ |   |
| `'NODE-DH'`<sup>1</sup> | ✔ | ✔ | ✔ |   |   |   |   | ✔ | ✔ |   |   |   |
| `'NODE-ED25519'`<sup>1</sup> | ✔ | ✔ | ✔ |   |   |   |   |   |   | ✔ | ✔ |   |
| `'NODE-ED448'`<sup>1</sup> | ✔ | ✔ | ✔ |   |   |   |   |   |   | ✔ | ✔ |   |

<sup>1</sup> Node.js-specific extension

## Class: `Crypto`
<!-- YAML
added: v15.0.0
-->

Calling `require('crypto').webcrypto` returns an instance of the `Crypto` class.
`Crypto` is a singleton that provides access to the remainder of the crypto API.

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

* `typedArray` {Buffer|TypedArray|DataView|ArrayBuffer}
* Returns: {Buffer|TypedArray|DataView|ArrayBuffer} Returns `typedArray`.

Generates cryptographically strong random values. The given `typedArray` is
filled with random values, and a reference to `typedArray` is returned.

An error will be thrown if the given `typedArray` is larger than 65,536 bytes.

## Class: `CryptoKey`
<!-- YAML
added: v15.0.0
-->

### `cryptoKey.algorithm`
<!-- YAML
added: v15.0.0
-->

<!--lint disable maximum-line-length remark-lint-->
* Type: {AesKeyGenParams|RsaHashedKeyGenParams|EcKeyGenParams|HmacKeyGenParams|NodeDsaKeyGenParams|NodeDhKeyGenParams}
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

* Type: {string[]}

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

|      Key Type        | `'encrypt'` | `'decrypt'` | `'sign'` | `'verify'` | `'deriveKey'` | `'deriveBits'` | `'wrapKey'` | `'unwrapKey'` |
| -------------------- | ----------- | ----------- | -------- | ---------- | ------------- | --------------- | ----------- | ------------- |
| `'AES-CBC'`           | ✔ | ✔ |   |   |   |   | ✔ |  ✔ |
| `'AES-CTR'`           | ✔ | ✔ |   |   |   |   | ✔ |  ✔ |
| `'AES-GCM'`           | ✔ | ✔ |   |   |   |   | ✔ |  ✔ |
| `'AES-KW'`            |   |   |   |   |   |   | ✔ | ✔ |
| `'ECDH'`              |   |   |   |   | ✔ | ✔ |   |   |
| `'ECDSA'`             |   |   | ✔ | ✔ |   |   |   |   |
| `'HDKF'`              |   |   |   |   | ✔ | ✔ |   |   |
| `'HMAC'`              |   |   | ✔ | ✔ |   |   |   |   |
| `'PBKDF2'`            |   |   |   |   | ✔ | ✔ |   |   |
| `'RSA-OAEP'`          | ✔ | ✔ |   |   |   |   | ✔ | ✔ |
| `'RSA-PSS'`           |   |   | ✔ | ✔ |   |   |   |   |
| `'RSASSA-PKCS1-v1_5'` |   |   | ✔ | ✔ |   |   |   |   |
| `'NODE-DSA'` <sup>1</sup> |   |   | ✔ | ✔ |   |   |   |   |
| `'NODE-DH'` <sup>1</sup> |   |   |   |   | ✔ | ✔ |   |   |
| `'NODE-SCRYPT'` <sup>1</sup> |   |   |   |   | ✔ | ✔ |   |   |
| `'NODE-ED25519'` <sup>1</sup> |   |   | ✔ | ✔ |   |   |   |   |
| `'NODE-ED448'` <sup>1</sup> |   |   | ✔ | ✔ |   |   |   |   |

<sup>1</sup> Node.js-specific extension.

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

### `subtle.decrypt(algorithm, key, data)`
<!-- YAML
added: v15.0.0
-->

* `algorithm`: {RsaOaepParams|AesCtrParams|AesCbcParams|AesGcmParams}
* `key`: {CryptoKey}
* `data`: {ArrayBuffer|TypedArray|DataView|Buffer}
* Returns: {Promise} containing {ArrayBuffer}

Using the method and parameters specified in `algorithm` and the keying
material provided by `key`, `subtle.decrypt()` attempts to decipher the
provided `data`. If successful, the returned promise will be resolved with
an {ArrayBuffer} containing the plaintext result.

The algorithms currently supported include:

* `'RSA-OAEP'`
* `'AES-CTR'`
* `'AES-CBC'`
* `'AES-GCM`'

### `subtle.deriveBits(algorithm, baseKey, length)`
<!-- YAML
added: v15.0.0
-->

<!--lint disable maximum-line-length remark-lint-->
* `algorithm`: {EcdhKeyDeriveParams|HkdfParams|Pbkdf2Params|NodeDhDeriveBitsParams|NodeScryptParams}
* `baseKey`: {CryptoKey}
* `length`: {number}
* Returns: {Promise} containing {ArrayBuffer}
<!--lint enable maximum-line-length remark-lint-->

Using the method and parameters specified in `algorithm` and the keying
material provided by `baseKey`, `subtle.deriveBits()` attempts to generate
`length` bits. The Node.js implementation requires that `length` is a
multiple of `8`. If successful, the returned promise will be resolved with
an {ArrayBuffer} containing the generated data.

The algorithms currently supported include:

* `'ECDH'`
* `'HKDF'`
* `'PBKDF2'`
* `'NODE-DH'`<sup>1</sup>
* `'NODE-SCRYPT'`<sup>1</sup>

<sup>1</sup> Node.js-specific extension

### `subtle.deriveKey(algorithm, baseKey, derivedKeyAlgorithm, extractable, keyUsages)`
<!-- YAML
added: v15.0.0
-->

<!--lint disable maximum-line-length remark-lint-->
* `algorithm`: {EcdhKeyDeriveParams|HkdfParams|Pbkdf2Params|NodeDhDeriveBitsParams|NodeScryptParams}
* `baseKey`: {CryptoKey}
* `derivedKeyAlgorithm`: {HmacKeyGenParams|AesKeyGenParams}
* `extractable`: {boolean}
* `keyUsages`: {string[]} See [Key usages][].
* Returns: {Promise} containing {CryptoKey}
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
* `'NODE-DH'`<sup>1</sup>
* '`NODE-SCRYPT'`<sup>1</sup>

<sup>1</sup> Node.js-specific extension

### `subtle.digest(algorithm, data)`
<!-- YAML
added: v15.0.0
-->

* `algorithm`: {string|Object}
* `data`: {ArrayBuffer|TypedArray|DataView|Buffer}
* Returns: {Promise} containing {ArrayBuffer}

Using the method identified by `algorithm`, `subtle.digest()` attempts to
generate a digest of `data`. If successful, the returned promise is resolved
with an {ArrayBuffer} containing the computed digest.

If `algorithm` is provided as a {string}, it must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`

If `algorithm` is provided as an {Object}, it must have a `name` property
whose value is one of the above.

### `subtle.encrypt(algorithm, key, data)`
<!-- YAML
added: v15.0.0
-->

* `algorithm`: {RsaOaepParams|AesCtrParams|AesCbcParams|AesGcmParams}
* `key`: {CryptoKey}
* Returns: {Promise} containing {ArrayBuffer}

Using the method and parameters specified by `algorithm` and the keying
material provided by `key`, `subtle.encrypt()` attempts to encipher `data`.
If successful, the returned promise is resolved with an {ArrayBuffer}
containing the encrypted result.

The algorithms currently supported include:

* `'RSA-OAEP'`
* `'AES-CTR'`
* `'AES-CBC'`
* `'AES-GCM`'

### `subtle.exportKey(format, key)`
<!-- YAML
added: v15.0.0
changes:
  - version: v15.9.0
    pr-url: https://github.com/nodejs/node/pull/37203
    description: Removed `'NODE-DSA'` JWK export.
-->

* `format`: {string} Must be one of `'raw'`, `'pkcs8'`, `'spki'`, `'jwk'`, or
  `'node.keyObject'`.
* `key`: {CryptoKey}
* Returns: {Promise} containing {ArrayBuffer}, or, if `format` is
  `'node.keyObject'`, a {KeyObject}.

Exports the given key into the specified format, if supported.

If the {CryptoKey} is not extractable, the returned promise will reject.

When `format` is either `'pkcs8'` or `'spki'` and the export is successful,
the returned promise will be resolved with an {ArrayBuffer} containing the
exported key data.

When `format` is `'jwk'` and the export is successful, the returned promise
will be resolved with a JavaScript object conforming to the [JSON Web Key][]
specification.

The special `'node.keyObject'` value for `format` is a Node.js-specific
extension that allows converting a {CryptoKey} into a Node.js {KeyObject}.

|      Key Type         | `'spki'` | `'pkcs8'` | `'jwk'` | `'raw'` |
| --------------------- | -------- | --------- | ------- | ------- |
| `'AES-CBC'`           |   |   | ✔ | ✔ |
| `'AES-CTR'`           |   |   | ✔ | ✔ |
| `'AES-GCM'`           |   |   | ✔ | ✔ |
| `'AES-KW'`            |   |   | ✔ | ✔ |
| `'ECDH'`              | ✔ | ✔ | ✔ | ✔ |
| `'ECDSA'`             | ✔ | ✔ | ✔ | ✔ |
| `'HDKF'`              |   |   |   |   |
| `'HMAC'`              |   |   | ✔ | ✔ |
| `'PBKDF2'`            |   |   |   |   |
| `'RSA-OAEP'`          | ✔ | ✔ | ✔ |   |
| `'RSA-PSS'`           | ✔ | ✔ | ✔ |   |
| `'RSASSA-PKCS1-v1_5'` | ✔ | ✔ | ✔ |   |
| `'NODE-DSA'` <sup>1</sup> | ✔ | ✔ |   |   |
| `'NODE-DH'` <sup>1</sup> | ✔ | ✔ |   |   |
| `'NODE-SCRYPT'` <sup>1</sup> |   |   |   |   |
| `'NODE-ED25519'` <sup>1</sup> | ✔ | ✔ | ✔ | ✔ |
| `'NODE-ED448'` <sup>1</sup> | ✔ | ✔ | ✔ | ✔ |

<sup>1</sup> Node.js-specific extension

### `subtle.generateKey(algorithm, extractable, keyUsages)`
<!-- YAML
added: v15.0.0
-->

<!--lint disable maximum-line-length remark-lint-->
* `algorithm`: {RsaHashedKeyGenParams|EcKeyGenParams|HmacKeyGenParams|AesKeyGenParams|NodeDsaKeyGenParams|NodeDhKeyGenParams|NodeEdKeyGenParams}
<!--lint enable maximum-line-length remark-lint-->
* `extractable`: {boolean}
* `keyUsages`: {string[]} See [Key usages][].
* Returns: {Promise} containing {CryptoKey|CryptoKeyPair}

Using the method and parameters provided in `algorithm`, `subtle.generateKey()`
attempts to generate new keying material. Depending the method used, the method
may generate either a single {CryptoKey} or a {CryptoKeyPair}.

The {CryptoKeyPair} (public and private key) generating algorithms supported
include:

* `'RSASSA-PKCS1-v1_5'`
* `'RSA-PSS'`
* `'RSA-OAEP'`
* `'ECDSA'`
* `'ECDH'`
* `'NODE-DSA'` <sup>1</sup>
* `'NODE-DH'` <sup>1</sup>
* `'NODE-ED25519'` <sup>1</sup>
* `'NODE-ED448'` <sup>1</sup>

The {CryptoKey} (secret key) generating algorithms supported include:

* `'HMAC'`
* `'AES-CTR'`
* `'AES-CBC'`
* `'AES-GCM'`
* `'AES-KW'`

<sup>1</sup> Non-standard Node.js extension

### `subtle.importKey(format, keyData, algorithm, extractable, keyUsages)`
<!-- YAML
added: v15.0.0
changes:
  - version: v15.9.0
    pr-url: https://github.com/nodejs/node/pull/37203
    description: Removed `'NODE-DSA'` JWK import.
-->

* `format`: {string} Must be one of `'raw'`, `'pkcs8'`, `'spki'`, `'jwk'`, or
  `'node.keyObject'`.
* `keyData`: {ArrayBuffer|TypedArray|DataView|Buffer|KeyObject}
<!--lint disable maximum-line-length remark-lint-->
* `algorithm`: {RsaHashedImportParams|EcKeyImportParams|HmacImportParams|AesImportParams|Pbkdf2ImportParams|NodeDsaImportParams|NodeDhImportParams|NodeScryptImportParams|NodeEdKeyImportParams}
<!--lint enable maximum-line-length remark-lint-->
* `extractable`: {boolean}
* `keyUsages`: {string[]} See [Key usages][].
* Returns: {Promise} containing {CryptoKey}

The `subtle.importKey()` method attempts to interpret the provided `keyData`
as the given `format` to create a {CryptoKey} instance using the provided
`algorithm`, `extractable`, and `keyUsages` arguments. If the import is
successful, the returned promise will be resolved with the created {CryptoKey}.

The special `'node.keyObject'` value for `format` is a Node.js-specific
extension that allows converting a Node.js {KeyObject} into a {CryptoKey}.

If importing a `'PBKDF2'` key, `extractable` must be `false`.

The algorithms currently supported include:

|      Key Type         | `'spki'` | `'pkcs8'` | `'jwk'` | `'raw'` |
| --------------------- | -------- | --------- | ------- | ------- |
| `'AES-CBC'`           |   |   | ✔ | ✔ |
| `'AES-CTR'`           |   |   | ✔ | ✔ |
| `'AES-GCM'`           |   |   | ✔ | ✔ |
| `'AES-KW'`            |   |   | ✔ | ✔ |
| `'ECDH'`              | ✔ | ✔ | ✔ | ✔ |
| `'ECDSA'`             | ✔ | ✔ | ✔ | ✔ |
| `'HDKF'`              |   |   |   | ✔ |
| `'HMAC'`              |   |   | ✔ | ✔ |
| `'PBKDF2'`            |   |   |   | ✔ |
| `'RSA-OAEP'`          | ✔ | ✔ | ✔ |   |
| `'RSA-PSS'`           | ✔ | ✔ | ✔ |   |
| `'RSASSA-PKCS1-v1_5'` | ✔ | ✔ | ✔ |   |
| `'NODE-DSA'` <sup>1</sup> | ✔ | ✔ |   |   |
| `'NODE-DH'` <sup>1</sup> | ✔ | ✔ |   |   |
| `'NODE-SCRYPT'` <sup>1</sup> |   |   |   | ✔ |
| `'NODE-ED25519'` <sup>1</sup> | ✔ | ✔ | ✔ | ✔ |
| `'NODE-ED448'` <sup>1</sup> | ✔ | ✔ | ✔ | ✔ |

<sup>1</sup> Node.js-specific extension

### `subtle.sign(algorithm, key, data)`
<!-- YAML
added: v15.0.0
-->

<!--lint disable maximum-line-length remark-lint-->
* `algorithm`: {RsaSignParams|RsaPssParams|EcdsaParams|HmacParams|NodeDsaSignParams}
* `key`: {CryptoKey}
* `data`: {ArrayBuffer|TypedArray|DataView|Buffer}
* Returns: {Promise} containing {ArrayBuffer}
<!--lint enable maximum-line-length remark-lint-->

Using the method and parameters given by `algorithm` and the keying material
provided by `key`, `subtle.sign()` attempts to generate a cryptographic
signature of `data`. If successful, the returned promise is resolved with
an {ArrayBuffer} containing the generated signature.

The algorithms currently supported include:

* `'RSASSA-PKCS1-v1_5'`
* `'RSA-PSS'`
* `'ECDSA'`
* `'HMAC'`
* `'NODE-DSA'`<sup>1</sup>
* `'NODE-ED25519'`<sup>1</sup>
* `'NODE-ED448'`<sup>1</sup>

<sup>1</sup> Non-standard Node.js extension

### `subtle.unwrapKey(format, wrappedKey, unwrappingKey, unwrapAlgo, unwrappedKeyAlgo, extractable, keyUsages)`
<!-- YAML
added: v15.0.0
-->

* `format`: {string} Must be one of `'raw'`, `'pkcs8'`, `'spki'`, or `'jwk'`.
* `wrappedKey`: {ArrayBuffer|TypedArray|DataView|Buffer}
* `unwrappingKey`: {CryptoKey}
<!--lint disable maximum-line-length remark-lint-->
* `unwrapAlgo`: {RsaOaepParams|AesCtrParams|AesCbcParams|AesGcmParams|AesKwParams}
* `unwrappedKeyAlgo`: {RsaHashedImportParams|EcKeyImportParams|HmacImportParams|AesImportParams}
<!--lint enable maximum-line-length remark-lint-->
* `extractable`: {boolean}
* `keyUsages`: {string[]} See [Key usages][].
* Returns: {Promise} containing {CryptoKey}

In cryptography, "wrapping a key" refers to exporting and then encrypting the
keying material. The `subtle.unwrapKey()` method attempts to decrypt a wrapped
key and create a {CryptoKey} instance. It is equivalent to calling
`subtle.decrypt()` first on the encrypted key data (using the `wrappedKey`,
`unwrapAlgo`, and `unwrappingKey` arguments as input) then passing the results
in to the `subtle.importKey()` method using the `unwrappedKeyAlgo`,
`extractable`, and `keyUsages` arguments as inputs. If successful, the returned
promise is resolved with a {CryptoKey} object.

The wrapping algorithms currently supported include:

* `'RSA-OAEP'`
* `'AES-CTR'`<sup>1</sup>
* `'AES-CBC'`<sup>1</sup>
* `'AES-GCM'`<sup>1</sup>
* `'AES-KW'`<sup>1</sup>

The unwrapped key algorithms supported include:

* `'RSASSA-PKCS1-v1_5'`
* `'RSA-PSS'`
* `'RSA-OAEP'`
* `'ECDSA'`
* `'ECDH'`
* `'HMAC'`
* `'AES-CTR'`
* `'AES-CBC'`
* `'AES-GCM'`
* `'AES-KW'`
* `'NODE-DSA'`<sup>1</sup>
* `'NODE-DH'`<sup>1</sup>

<sup>1</sup> Non-standard Node.js extension

### `subtle.verify(algorithm, key, signature, data)`
<!-- YAML
added: v15.0.0
-->

<!--lint disable maximum-line-length remark-lint-->
* `algorithm`: {RsaSignParams|RsaPssParams|EcdsaParams|HmacParams|NodeDsaSignParams}
* `key`: {CryptoKey}
* `signature`: {ArrayBuffer|TypedArray|DataView|Buffer}
* `data`: {ArrayBuffer|TypedArray|DataView|Buffer}
* Returns: {Promise} containing {boolean}
<!--lint enable maximum-line-length remark-lint-->

Using the method and parameters given in `algorithm` and the keying material
provided by `key`, `subtle.verify()` attempts to verify that `signature` is
a valid cryptographic signature of `data`. The returned promise is resolved
with either `true` or `false`.

The algorithms currently supported include:

* `'RSASSA-PKCS1-v1_5'`
* `'RSA-PSS'`
* `'ECDSA'`
* `'HMAC'`
* `'NODE-DSA'`<sup>1</sup>
* `'NODE-ED25519'`<sup>1</sup>
* `'NODE-ED448'`<sup>1</sup>

<sup>1</sup> Non-standard Node.js extension

### `subtle.wrapKey(format, key, wrappingKey, wrapAlgo)`
<!-- YAML
added: v15.0.0
-->

* `format`: {string} Must be one of `'raw'`, `'pkcs8'`, `'spki'`, or `'jwk'`.
* `key`: {CryptoKey}
* `wrappingKey`: {CryptoKey}
* `wrapAlgo`: {RsaOaepParams|AesCtrParams|AesCbcParams|AesGcmParams|AesKwParams}
* Returns: {Promise} containing {ArrayBuffer}

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

* `'RSA-OAEP'`
* `'AES-CTR'`
* `'AES-CBC'`
* `'AES-GCM'`
* `'AES-KW'`

## Algorithm Parameters

The algorithm parameter objects define the methods and parameters used by
the various {SubtleCrypto} methods. While described here as "classes", they
are simple JavaScript dictionary objects.

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

### Class: `AesGcmParams`
<!-- YAML
added: v15.0.0
-->

#### `aesGcmParams.additionalData`
<!-- YAML
added: v15.0.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer|undefined}

With the AES-GCM method, the `additionalData` is extra input that is not
encrypted but is included in the authentication of the data. The use of
`additionalData` is optional.

#### `aesGcmParams.iv`
<!-- YAML
added: v15.0.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer}

The initialization vector must be unique for every encryption operation
using a given key. It is recommended by the AES-GCM specification that
this contain at least 12 random bytes.

#### `aesGcmParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'AES-GCM'`.

#### `aesGcmParams.tagLength`
<!-- YAML
added: v15.0.0
-->

* Type: {number} The size in bits of the generated authentication tag.
  This values must be one of `32`, `64`, `96`, `104`, `112`, `120`, or
  `128`. **Default:** `128`.

### Class: `AesImportParams`
<!-- YAML
added: v15.0.0
-->

#### `aesImportParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be one of `'AES-CTR'`, `'AES-CBC'`, `'AES-GCM'`, or
  `'AES-KW'`.

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

### Class: `AesKwParams`
<!-- YAML
added: v15.0.0
-->

#### `aesKwParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'AES-KW'`.

### Class: `EcdhKeyDeriveParams`
<!-- YAML
added: v15.0.0
-->

#### `ecdhKeyDeriveParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'ECDH'`.

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
-->

* Type: {string|Object}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`

If represented as an {Object}, the object must have a `name` property
whose value is one of the above listed values.

#### `ecdsaParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'ECDSA'`.

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

* Type: {string} Must be one of `'P-256'`, `'P-384'`, `'P-521'`,
  `'NODE-ED25519'`, `'NODE-ED448'`, `'NODE-X25519'`, or `'NODE-X448'`.

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

* Type: {string} Must be one of `'P-256'`, `'P-384'`, `'P-521'`,
  `'NODE-ED25519'`, `'NODE-ED448'`, `'NODE-X25519'`, or `'NODE-X448'`.

### Class: `HkdfParams`
<!-- YAML
added: v15.0.0
-->

#### `hkdfParams.hash`
<!-- YAML
added: v15.0.0
-->

* Type: {string|Object}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`

If represented as an {Object}, the object must have a `name` property
whose value is one of the above listed values.

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

#### 'hmacImportParams.hash`
<!-- YAML
added: v15.0.0
-->

* Type: {string|Object}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`

If represented as an {Object}, the object must have a `name` property
whose value is one of the above listed values.

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

### Class: `HmacKeyGenParams`
<!-- YAML
added: v15.0.0
-->

#### `hmacKeyGenParams.hash`
<!-- YAML
added: v15.0.0
-->

* Type: {string|Object}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`

If represented as an {Object}, the object must have a `name` property
whose value is one of the above listed values.

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

### Class: `HmacParams`
<!-- YAML
added: v15.0.0
-->

#### `hmacParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'HMAC`.

### Class: `Pbkdf2ImportParams`
<!-- YAML
added: v15.0.0
-->

#### `pbkdf2ImportParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'PBKDF2'`

### Class: `Pbkdf2Params`
<!-- YAML
added: v15.0.0
-->

#### `pbkdb2Params.hash`
<!-- YAML
added: v15.0.0
-->

* Type: {string|Object}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`

If represented as an {Object}, the object must have a `name` property
whose value is one of the above listed values.

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
-->

* Type: {string|Object}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`

If represented as an {Object}, the object must have a `name` property
whose value is one of the above listed values.

#### `rsaHashedImportParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be one of `'RSASSA-PKCS1-v1_5'`, `'RSA-PSS'`, or
  `'RSA-OAEP'`.

### Class: `RsaHashedKeyGenParams`
<!-- YAML
added: v15.0.0
-->

#### `rsaHashedKeyGenParams.hash`
<!-- YAML
added: v15.0.0
-->

* Type: {string|Object}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`

If represented as an {Object}, the object must have a `name` property
whose value is one of the above listed values.

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

#### rsaOaepParams.label
<!-- YAML
added: v15.0.0
-->

* Type: {ArrayBuffer|TypedArray|DataView|Buffer}

An additional collection of bytes that will not be encrypted, but will be bound
to the generated ciphertext.

The `rsaOaepParams.label` parameter is optional.

#### rsaOaepParams.name
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

### Class: `RsaSignParams`
<!-- YAML
added: v15.0.0
-->

#### `rsaSignParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'RSASSA-PKCS1-v1_5'`

## Node.js-specific extensions

The Node.js Web Crypto API extends various aspects of the Web Crypto API.
These extensions are consistently identified by prepending names with the
`node.` prefix. For instance, the `'node.keyObject'` key format can be
used with the `subtle.exportKey()` and `subtle.importKey()` methods to
convert between a WebCrypto {CryptoKey} object and a Node.js {KeyObject}.

Care should be taken when using Node.js-specific extensions as they are
not supported by other WebCrypto implementations and reduce the portability
of code to other environments.

### `NODE-DH` Algorithm
<!-- YAML
added: v15.0.0
-->

The `NODE-DH` algorithm is the common implementation of Diffie-Hellman
key agreement.

#### Class: `NodeDhImportParams`
<!-- YAML
added: v15.0.0
-->

##### `nodeDhImportParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'NODE-DH'`.

#### Class: NodeDhKeyGenParams`
<!-- YAML
added: v15.0.0
-->

##### `nodeDhKeyGenParams.generator`
<!-- YAML
added: v15.0.0
-->

* Type: {number} A custom generator.

##### `nodeDhKeyGenParams.group`
<!-- YAML
added: v15.0.0
-->

* Type: {string} The Diffie-Hellman group name.

##### `nodeDhKeyGenParams.prime`
<!-- YAML
added: v15.0.0
-->

* Type: {Buffer} The prime parameter.

##### `nodeDhKeyGenParams.primeLength`
<!-- YAML
added: v15.0.0
-->

* Type: {number} The length in bits of the prime.

#### Class: NodeDhDeriveBitsParams
<!-- YAML
added: v15.0.0
-->

##### `nodeDhDeriveBitsParams.public`
<!-- YAML
added: v15.0.0
-->

* Type: {CryptoKey} The other parties public key.

### `NODE-DSA` Algorithm
<!-- YAML
added: v15.0.0
-->

The `NODE-DSA` algorithm is the common implementation of the DSA digital
signature algorithm.

#### Class: `NodeDsaImportParams`
<!-- YAML
added: v15.0.0
-->

##### `nodeDsaImportParams.hash`
<!-- YAML
added: v15.0.0
-->

* Type: {string|Object}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`

If represented as an {Object}, the object must have a `name` property
whose value is one of the above listed values.

##### `nodeDsaImportParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'NODE-DSA'`.

#### Class: `NodeDsaKeyGenParams`
<!-- YAML
added: v15.0.0
-->

##### `nodeDsaKeyGenParams.divisorLength`
<!-- YAML
added: v15.0.0
-->

* Type: {number}

The optional length in bits of the DSA divisor.

##### `nodeDsaKeyGenParams.hash`
<!-- YAML
added: v15.0.0
-->

* Type: {string|Object}

If represented as a {string}, the value must be one of:

* `'SHA-1'`
* `'SHA-256'`
* `'SHA-384'`
* `'SHA-512'`

If represented as an {Object}, the object must have a `name` property
whose value is one of the above listed values.

##### `nodeDsaKeyGenParams.modulusLength`
<!-- YAML
added: v15.0.0
-->

* Type: {number}

The length in bits of the DSA modulus. As a best practice, this should be
at least `2048`.

##### `nodeDsaKeyGenParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'NODE-DSA'`.

#### Class: `NodeDsaSignParams`
<!-- YAML
added: v15.0.0
-->

##### `nodeDsaSignParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'NODE-DSA'`

### `NODE-ED25519` and `NODE-ED448` Algorithms
<!-- YAML
added: v15.8.0
-->

#### Class: `NodeEdKeyGenParams`
<!-- YAML
added: v15.8.0
-->

##### `nodeEdKeyGenParams.name`
<!-- YAML
added: v15.8.0
-->

* Type: {string} Must be one of `'NODE-ED25519'`, `'NODE-ED448'` or `'ECDH'`.

##### `nodeEdKeyGenParams.namedCurve`
<!-- YAML
added: v15.8.0
-->

* Type: {string} Must be one of `'NODE-ED25519'`, `'NODE-ED448'`,
  `'NODE-X25519'`, or `'NODE-X448'`.

#### Class: `NodeEdKeyImportParams`
<!-- YAML
added: v15.8.0
-->

##### `nodeEdKeyImportParams.name`
<!-- YAML
added: v15.8.0
-->

* Type: {string} Must be one of `'NODE-ED25519'` or `'NODE-ED448'`
  if importing an `Ed25519` or `Ed448` key, or `'ECDH'` if importing
  an `X25519` or `X448` key.

##### `nodeEdKeyImportParams.namedCurve`
<!-- YAML
added: v15.8.0
-->

* Type: {string} Must be one of `'NODE-ED25519'`, `'NODE-ED448'`,
  `'NODE-X25519'`, or `'NODE-X448'`.

##### `nodeEdKeyImportParams.public`
<!-- YAML
added: v15.8.0
-->

* Type: {boolean}

The `public` parameter is used to specify that the `'raw'` format key is to be
interpreted as a public key. **Default:** `false`.

### `NODE-SCRYPT` Algorithm
<!-- YAML
added: v15.0.0
-->

The `NODE-SCRYPT` algorithm is the common implementation of the scrypt key
derivation algorithm.

#### Class: `NodeScryptImportParams`
<!-- YAML
added: v15.0.0
-->

##### `nodeScryptImportParams.name`
<!-- YAML
added: v15.0.0
-->

* Type: {string} Must be `'NODE-SCRYPT'`.

#### Class: `NodeScryptParams`
<!-- YAML
added: v15.0.0
-->

##### `nodeScryptParams.encoding`
<!-- YAML
added: v15.0.0
-->

* Type: {string} The string encoding when `salt` is a string.

##### `nodeScryptParams.maxmem`
<!-- YAML
added: v15.0.0
-->

* Type: {number} Memory upper bound. It is an error when (approximately)
  `127 * N * r > maxmem`. **Default:** `32 * 1024 * 1024`.

##### `nodeScryptParams.N`
<!-- YAML
added: v15.0.0
-->

* Type: {number} The CPU/memory cost parameter. Must e a power of two
  greater than 1. **Default:** `16384`.

##### `nodeScryptParams.p`
<!-- YAML
added: v15.0.0
-->

* Type: {number} Parallelization parameter. **Default:** `1`.

##### `nodeScryptParams.r`
<!-- YAML
added: v15.0.0
-->

* Type: {number} Block size parameter. **Default:** `8`.

##### `nodeScryptParams.salt`
<!-- YAML
added: v15.0.0
-->

* Type: {string|ArrayBuffer|Buffer|TypedArray|DataView}

[JSON Web Key]: https://tools.ietf.org/html/rfc7517
[Key usages]: #webcrypto_cryptokey_usages
[Web Crypto API]: https://www.w3.org/TR/WebCryptoAPI/
