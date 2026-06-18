'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeSlice,
  FunctionPrototypeCall,
  JSONParse,
  JSONStringify,
  ObjectDefineProperties,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  ObjectSetPrototypeOf,
  PromiseReject,
  PromiseResolve,
  ReflectApply,
  ReflectConstruct,
  SafeArrayIterator,
  StringPrototypeRepeat,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  SymbolIterator,
  SymbolToStringTag,
  TypedArrayPrototypeGetBuffer,
} = primordials;

const {
  CShakeJob,
  kWebCryptoKeyFormatRaw,
  kWebCryptoKeyFormatPKCS8,
  kWebCryptoKeyFormatSPKI,
  kWebCryptoCipherEncrypt,
  kWebCryptoCipherDecrypt,
} = internalBinding('crypto');

const {
  decodeUTF8,
  encodeUtf8String,
} = internalBinding('encoding_binding');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  CryptoKey,
  getCryptoKeyAlgorithm,
  getCryptoKeyExtractable,
  getCryptoKeyHandle,
  getCryptoKeyType,
  getCryptoKeyUsages,
  getCryptoKeyUsagesMask,
  hasCryptoKeyUsage,
  importGenericSecretKey,
  toPublicCryptoKey,
} = require('internal/crypto/keys');

const {
  asyncDigest,
} = require('internal/crypto/hash');

const {
  cleanupWebCryptoResult,
  getBlockSize,
  jobPromiseThen,
  normalizeAlgorithm,
  normalizeHashName,
  numBitsToBytes,
  prepareWebCryptoResult,
  validateMaxBufferLength,
} = require('internal/crypto/util');

const {
  emitExperimentalWarning,
  kEnumerableProperty,
  lazyDOMException,
  setOwnProperty,
} = require('internal/util');

const {
  getRandomValues: _getRandomValues,
  randomUUID: _randomUUID,
} = require('internal/crypto/random');

const {
  isPromise,
} = require('internal/util/types');

let webidl;

// WebCrypto methods return promises, including for synchronous validation
// failures. Keep that conversion in one place so method bodies stay readable.
function callSubtleCryptoMethod(fn, receiver, args) {
  try {
    const result = ReflectApply(fn, receiver, args);
    if (isPromise(result))
      return result;
    // PromiseResolve() performs thenable assimilation for object results.
    // Shadow inherited then accessors while it resolves synchronous results.
    const shouldCleanupResult = prepareWebCryptoResult(result);
    try {
      return PromiseResolve(result);
    } finally {
      if (shouldCleanupResult)
        cleanupWebCryptoResult(result);
    }
  } catch (err) {
    return PromiseReject(err);
  }
}

const kArgumentContexts = [
  '1st argument',
  '2nd argument',
  '3rd argument',
  '4th argument',
  '5th argument',
  '6th argument',
  '7th argument',
];

function prepareSubtleMethod(receiver, method, argLength, required) {
  if (receiver !== subtle) throw new ERR_INVALID_THIS('SubtleCrypto');

  webidl ??= require('internal/crypto/webidl');
  const prefix = `Failed to execute '${method}' on 'SubtleCrypto'`;
  webidl.requiredArguments(argLength, required, { prefix });
  return prefix;
}

function convertSubtleArgument(prefix, converter, value, index) {
  return webidl.converters[converter](value, {
    prefix,
    context: kArgumentContexts[index],
  });
}

function digest(algorithm, data) {
  return callSubtleCryptoMethod(digestImpl, this, arguments);
}

function digestImpl(algorithm, data) {
  const prefix = prepareSubtleMethod(this, 'digest', arguments.length, 2);
  let i = 0;
  algorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', algorithm, i++);
  data = convertSubtleArgument(prefix, 'BufferSource', data, i++);

  const normalizedAlgorithm = normalizeAlgorithm(algorithm, 'digest');

  return FunctionPrototypeCall(asyncDigest, this, normalizedAlgorithm, data);
}

function randomUUID() {
  if (this !== crypto) throw new ERR_INVALID_THIS('Crypto');
  return _randomUUID();
}

function generateKey(
  algorithm,
  extractable,
  keyUsages) {
  return callSubtleCryptoMethod(generateKeyImpl, this, arguments);
}

function generateKeyImpl(
  algorithm,
  extractable,
  keyUsages) {
  const prefix = prepareSubtleMethod(this, 'generateKey', arguments.length, 3);
  let i = 0;
  algorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', algorithm, i++);
  extractable = convertSubtleArgument(prefix, 'boolean', extractable, i++);
  const usages = convertSubtleArgument(
    prefix, 'sequence<KeyUsage>', keyUsages, i++);

  const normalizedAlgorithm = normalizeAlgorithm(algorithm, 'generateKey');
  switch (normalizedAlgorithm.name) {
    case 'RSASSA-PKCS1-v1_5':
      // Fall through
    case 'RSA-PSS':
      // Fall through
    case 'RSA-OAEP':
      return require('internal/crypto/rsa')
        .rsaKeyGenerate(normalizedAlgorithm, extractable, usages);
    case 'Ed25519':
      // Fall through
    case 'Ed448':
      // Fall through
    case 'X25519':
      // Fall through
    case 'X448':
      return require('internal/crypto/cfrg')
        .cfrgGenerateKey(normalizedAlgorithm, extractable, usages);
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      return require('internal/crypto/ec')
        .ecGenerateKey(normalizedAlgorithm, extractable, usages);
    case 'HMAC':
      return require('internal/crypto/mac')
        .hmacGenerateKey(normalizedAlgorithm, extractable, usages);
    case 'AES-CTR':
      // Fall through
    case 'AES-CBC':
      // Fall through
    case 'AES-GCM':
      // Fall through
    case 'AES-OCB':
      // Fall through
    case 'AES-KW':
      return require('internal/crypto/aes')
        .aesGenerateKey(normalizedAlgorithm, extractable, usages);
    case 'ChaCha20-Poly1305':
      return require('internal/crypto/chacha20_poly1305')
        .c20pGenerateKey(normalizedAlgorithm, extractable, usages);
    case 'ML-DSA-44':
      // Fall through
    case 'ML-DSA-65':
      // Fall through
    case 'ML-DSA-87':
      return require('internal/crypto/ml_dsa')
        .mlDsaGenerateKey(normalizedAlgorithm, extractable, usages);
    case 'ML-KEM-512':
      // Fall through
    case 'ML-KEM-768':
      // Fall through
    case 'ML-KEM-1024':
      return require('internal/crypto/ml_kem')
        .mlKemGenerateKey(normalizedAlgorithm, extractable, usages);
    case 'KMAC128':
      // Fall through
    case 'KMAC256':
      return require('internal/crypto/mac')
        .kmacGenerateKey(normalizedAlgorithm, extractable, usages);
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }
}

function deriveBits(algorithm, baseKey, length = null) {
  return callSubtleCryptoMethod(deriveBitsImpl, this, arguments);
}

function deriveBitsImpl(algorithm, baseKey, length = null) {
  const prefix = prepareSubtleMethod(this, 'deriveBits', arguments.length, 2);
  let i = 0;
  algorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', algorithm, i++);
  baseKey = convertSubtleArgument(prefix, 'CryptoKey', baseKey, i++);
  if (length !== null) {
    length = convertSubtleArgument(prefix, 'unsigned long', length, i++);
  }

  const normalizedAlgorithm = normalizeAlgorithm(algorithm, 'deriveBits');
  if (!hasCryptoKeyUsage(baseKey, 'deriveBits')) {
    throw lazyDOMException(
      'baseKey does not have deriveBits usage',
      'InvalidAccessError');
  }
  if (getCryptoKeyAlgorithm(baseKey).name !== normalizedAlgorithm.name)
    throw lazyDOMException('Key algorithm mismatch', 'InvalidAccessError');
  switch (normalizedAlgorithm.name) {
    case 'X25519':
      // Fall through
    case 'X448':
      // Fall through
    case 'ECDH':
      return require('internal/crypto/diffiehellman')
        .ecdhDeriveBits(normalizedAlgorithm, baseKey, length);
    case 'HKDF':
      return require('internal/crypto/hkdf')
        .hkdfDeriveBits(normalizedAlgorithm, baseKey, length);
    case 'PBKDF2':
      return require('internal/crypto/pbkdf2')
        .pbkdf2DeriveBits(normalizedAlgorithm, baseKey, length);
    case 'Argon2d':
      // Fall through
    case 'Argon2i':
      // Fall through
    case 'Argon2id':
      return require('internal/crypto/argon2')
        .argon2DeriveBits(normalizedAlgorithm, baseKey, length);
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }
}

function getKeyLength({ name, length, hash }) {
  switch (name) {
    case 'AES-CTR':
    case 'AES-CBC':
    case 'AES-GCM':
    case 'AES-OCB':
    case 'AES-KW':
      if (length !== 128 && length !== 192 && length !== 256)
        throw lazyDOMException('Invalid key length', 'OperationError');

      return length;
    case 'HMAC':
      if (length === undefined) {
        return getBlockSize(hash?.name);
      }

      if (typeof length === 'number' && length !== 0) {
        return length;
      }

      throw lazyDOMException('Invalid key length', 'OperationError');
    case 'KMAC128':
    case 'KMAC256':
      if (typeof length === 'number') {
        return length;
      }

      return name === 'KMAC128' ? 128 : 256;
    case 'HKDF':
    case 'PBKDF2':
    case 'Argon2d':
    case 'Argon2i':
    case 'Argon2id':
      return null;
    case 'ChaCha20-Poly1305':
      return 256;
  }
}

function deriveKey(
  algorithm,
  baseKey,
  derivedKeyType,
  extractable,
  keyUsages) {
  return callSubtleCryptoMethod(deriveKeyImpl, this, arguments);
}

function deriveKeyImpl(
  algorithm,
  baseKey,
  derivedKeyType,
  extractable,
  keyUsages) {
  const prefix = prepareSubtleMethod(this, 'deriveKey', arguments.length, 5);
  let i = 0;
  algorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', algorithm, i++);
  baseKey = convertSubtleArgument(prefix, 'CryptoKey', baseKey, i++);
  derivedKeyType = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', derivedKeyType, i++);
  extractable = convertSubtleArgument(prefix, 'boolean', extractable, i++);
  const usages = convertSubtleArgument(
    prefix, 'sequence<KeyUsage>', keyUsages, i++);

  const normalizedAlgorithm = normalizeAlgorithm(algorithm, 'deriveBits');
  const normalizedDerivedKeyAlgorithmImport =
    normalizeAlgorithm(derivedKeyType, 'importKey');
  const normalizedDerivedKeyAlgorithmLength =
    normalizeAlgorithm(derivedKeyType, 'get key length');
  if (!hasCryptoKeyUsage(baseKey, 'deriveKey')) {
    throw lazyDOMException(
      'baseKey does not have deriveKey usage',
      'InvalidAccessError');
  }
  if (getCryptoKeyAlgorithm(baseKey).name !== normalizedAlgorithm.name)
    throw lazyDOMException('Key algorithm mismatch', 'InvalidAccessError');

  const length = getKeyLength(normalizedDerivedKeyAlgorithmLength);
  let secret;
  switch (normalizedAlgorithm.name) {
    case 'X25519':
      // Fall through
    case 'X448':
      // Fall through
    case 'ECDH':
      secret = require('internal/crypto/diffiehellman')
        .ecdhDeriveBits(normalizedAlgorithm, baseKey, length);
      break;
    case 'HKDF':
      secret = require('internal/crypto/hkdf')
        .hkdfDeriveBits(normalizedAlgorithm, baseKey, length);
      break;
    case 'PBKDF2':
      secret = require('internal/crypto/pbkdf2')
        .pbkdf2DeriveBits(normalizedAlgorithm, baseKey, length);
      break;
    case 'Argon2d':
      // Fall through
    case 'Argon2i':
      // Fall through
    case 'Argon2id':
      secret = require('internal/crypto/argon2')
        .argon2DeriveBits(normalizedAlgorithm, baseKey, length);
      break;
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }

  return jobPromiseThen(secret, (secret) => FunctionPrototypeCall(
    importKeySync,
    this,
    'raw-secret',
    secret,
    normalizedDerivedKeyAlgorithmImport,
    extractable,
    usages,
  ));
}

function exportKeySpki(key) {
  switch (getCryptoKeyAlgorithm(key).name) {
    case 'RSASSA-PKCS1-v1_5':
      // Fall through
    case 'RSA-PSS':
      // Fall through
    case 'RSA-OAEP':
      return require('internal/crypto/rsa')
        .rsaExportKey(key, kWebCryptoKeyFormatSPKI);
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      return require('internal/crypto/ec')
        .ecExportKey(key, kWebCryptoKeyFormatSPKI);
    case 'Ed25519':
      // Fall through
    case 'Ed448':
      // Fall through
    case 'X25519':
      // Fall through
    case 'X448':
      return require('internal/crypto/cfrg')
        .cfrgExportKey(key, kWebCryptoKeyFormatSPKI);
    case 'ML-DSA-44':
      // Fall through
    case 'ML-DSA-65':
      // Fall through
    case 'ML-DSA-87':
      return require('internal/crypto/ml_dsa')
        .mlDsaExportKey(key, kWebCryptoKeyFormatSPKI);
    case 'ML-KEM-512':
      // Fall through
    case 'ML-KEM-768':
      // Fall through
    case 'ML-KEM-1024':
      return require('internal/crypto/ml_kem')
        .mlKemExportKey(key, kWebCryptoKeyFormatSPKI);
    default:
      return undefined;
  }
}

function exportKeyPkcs8(key) {
  switch (getCryptoKeyAlgorithm(key).name) {
    case 'RSASSA-PKCS1-v1_5':
      // Fall through
    case 'RSA-PSS':
      // Fall through
    case 'RSA-OAEP':
      return require('internal/crypto/rsa')
        .rsaExportKey(key, kWebCryptoKeyFormatPKCS8);
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      return require('internal/crypto/ec')
        .ecExportKey(key, kWebCryptoKeyFormatPKCS8);
    case 'Ed25519':
      // Fall through
    case 'Ed448':
      // Fall through
    case 'X25519':
      // Fall through
    case 'X448':
      return require('internal/crypto/cfrg')
        .cfrgExportKey(key, kWebCryptoKeyFormatPKCS8);
    case 'ML-DSA-44':
      // Fall through
    case 'ML-DSA-65':
      // Fall through
    case 'ML-DSA-87':
      return require('internal/crypto/ml_dsa')
        .mlDsaExportKey(key, kWebCryptoKeyFormatPKCS8);
    case 'ML-KEM-512':
      // Fall through
    case 'ML-KEM-768':
      // Fall through
    case 'ML-KEM-1024':
      return require('internal/crypto/ml_kem')
        .mlKemExportKey(key, kWebCryptoKeyFormatPKCS8);
    default:
      return undefined;
  }
}

function exportKeyRawPublic(key, format) {
  switch (getCryptoKeyAlgorithm(key).name) {
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      return require('internal/crypto/ec')
        .ecExportKey(key, kWebCryptoKeyFormatRaw);
    case 'Ed25519':
      // Fall through
    case 'Ed448':
      // Fall through
    case 'X25519':
      // Fall through
    case 'X448':
      return require('internal/crypto/cfrg')
        .cfrgExportKey(key, kWebCryptoKeyFormatRaw);
    case 'ML-DSA-44':
      // Fall through
    case 'ML-DSA-65':
      // Fall through
    case 'ML-DSA-87': {
      // ML-DSA keys don't recognize "raw"
      if (format !== 'raw-public') {
        return undefined;
      }
      return require('internal/crypto/ml_dsa')
        .mlDsaExportKey(key, kWebCryptoKeyFormatRaw);
    }
    case 'ML-KEM-512':
      // Fall through
    case 'ML-KEM-768':
      // Fall through
    case 'ML-KEM-1024': {
      // ML-KEM keys don't recognize "raw"
      if (format !== 'raw-public') {
        return undefined;
      }
      return require('internal/crypto/ml_kem')
        .mlKemExportKey(key, kWebCryptoKeyFormatRaw);
    }
    default:
      return undefined;
  }
}

function exportKeyRawSeed(key) {
  switch (getCryptoKeyAlgorithm(key).name) {
    case 'ML-DSA-44':
      // Fall through
    case 'ML-DSA-65':
      // Fall through
    case 'ML-DSA-87':
      return require('internal/crypto/ml_dsa')
        .mlDsaExportKey(key, kWebCryptoKeyFormatRaw);
    case 'ML-KEM-512':
      // Fall through
    case 'ML-KEM-768':
      // Fall through
    case 'ML-KEM-1024':
      return require('internal/crypto/ml_kem')
        .mlKemExportKey(key, kWebCryptoKeyFormatRaw);
    default:
      return undefined;
  }
}

function exportKeyRawSecret(key, format) {
  switch (getCryptoKeyAlgorithm(key).name) {
    case 'AES-CTR':
      // Fall through
    case 'AES-CBC':
      // Fall through
    case 'AES-GCM':
      // Fall through
    case 'AES-KW':
      // Fall through
    case 'HMAC':
      return TypedArrayPrototypeGetBuffer(getCryptoKeyHandle(key).export());
    case 'AES-OCB':
      // Fall through
    case 'KMAC128':
      // Fall through
    case 'KMAC256':
      // Fall through
    case 'ChaCha20-Poly1305':
      if (format === 'raw-secret') {
        return TypedArrayPrototypeGetBuffer(getCryptoKeyHandle(key).export());
      }
      return undefined;
    default:
      return undefined;
  }
}

function exportKeyJWK(key) {
  const algorithm = getCryptoKeyAlgorithm(key);
  let alg;
  switch (algorithm.name) {
    case 'RSASSA-PKCS1-v1_5': {
      alg = normalizeHashName(
        algorithm.hash.name,
        normalizeHashName.kContextJwkRsa);
      break;
    }
    case 'RSA-PSS': {
      alg = normalizeHashName(
        algorithm.hash.name,
        normalizeHashName.kContextJwkRsaPss);
      break;
    }
    case 'RSA-OAEP': {
      alg = normalizeHashName(
        algorithm.hash.name,
        normalizeHashName.kContextJwkRsaOaep);
      break;
    }
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      // Fall through
    case 'X25519':
      // Fall through
    case 'X448':
      // Fall through
    case 'ML-DSA-44':
      // Fall through
    case 'ML-DSA-65':
      // Fall through
    case 'ML-DSA-87':
      // Fall through
    case 'ML-KEM-512':
      // Fall through
    case 'ML-KEM-768':
      // Fall through
    case 'ML-KEM-1024':
      break;
    case 'Ed25519':
      // Fall through
    case 'Ed448':
      alg = algorithm.name;
      break;
    case 'AES-CTR':
      // Fall through
    case 'AES-CBC':
      // Fall through
    case 'AES-GCM':
      // Fall through
    case 'AES-OCB':
      // Fall through
    case 'AES-KW':
      alg = require('internal/crypto/aes')
        .getAlgorithmName(algorithm.name, algorithm.length);
      break;
    case 'ChaCha20-Poly1305':
      alg = 'C20P';
      break;
    case 'HMAC': {
      alg = normalizeHashName(
        algorithm.hash.name,
        normalizeHashName.kContextJwkHmac);
      break;
    }
    case 'KMAC128':
      alg = 'K128';
      break;
    case 'KMAC256': {
      alg = 'K256';
      break;
    }
    default:
      return undefined;
  }

  // Keep `alg` in the object literal so an inherited setter cannot capture
  // `parameters` before native export populates key material. Delete it for
  // algorithms without a JWK alg value to keep the expected shape.
  const parameters = {
    key_ops: ArrayPrototypeSlice(getCryptoKeyUsages(key), 0),
    ext: getCryptoKeyExtractable(key),
    alg,
  };
  if (alg === undefined) delete parameters.alg;

  return getCryptoKeyHandle(key).exportJwk(parameters, true);
}

function exportKeySync(format, key) {
  const algorithm = getCryptoKeyAlgorithm(key);
  try {
    normalizeAlgorithm(algorithm, 'exportKey');
  } catch {
    throw lazyDOMException(
      `${algorithm.name} key export is not supported`, 'NotSupportedError');
  }

  if (!getCryptoKeyExtractable(key))
    throw lazyDOMException('key is not extractable', 'InvalidAccessError');

  const type = getCryptoKeyType(key);
  let result;
  switch (format) {
    case 'spki': {
      if (type === 'public') {
        result = exportKeySpki(key);
      }
      break;
    }
    case 'pkcs8': {
      if (type === 'private') {
        result = exportKeyPkcs8(key);
      }
      break;
    }
    case 'jwk': {
      result = exportKeyJWK(key);
      break;
    }
    case 'raw-secret': {
      if (type === 'secret') {
        result = exportKeyRawSecret(key, format);
      }
      break;
    }
    case 'raw-public': {
      if (type === 'public') {
        result = exportKeyRawPublic(key, format);
      }
      break;
    }
    case 'raw-seed': {
      if (type === 'private') {
        result = exportKeyRawSeed(key);
      }
      break;
    }
    case 'raw': {
      if (type === 'secret') {
        result = exportKeyRawSecret(key, format);
      } else if (type === 'public') {
        result = exportKeyRawPublic(key, format);
      }
      break;
    }
  }

  if (!result) {
    throw lazyDOMException(
      `Unable to export ${algorithm.name} ${type} key using ${format} format`,
      'NotSupportedError');
  }

  return result;
}

function exportKey(format, key) {
  return callSubtleCryptoMethod(exportKeyImpl, this, arguments);
}

function exportKeyImpl(format, key) {
  const prefix = prepareSubtleMethod(this, 'exportKey', arguments.length, 2);
  let i = 0;
  format = convertSubtleArgument(prefix, 'KeyFormat', format, i++);
  key = convertSubtleArgument(prefix, 'CryptoKey', key, i++);

  return exportKeySync(format, key);
}

// Parsed JWK arrays are detached from Array.prototype but still need to pass
// WebIDL sequence conversion, which reads @@iterator from the value.
function safeArrayIterator() {
  return new SafeArrayIterator(this);
}

// The WebCrypto spec parses and stringifies JWKs in a fresh global object.
// Detach internal JSON values from the current global's mutable prototypes to
// approximate those fresh-realm semantics without creating a new realm.
function detachFromUserPrototypes(value) {
  if (value === null || typeof value !== 'object')
    return;

  ObjectSetPrototypeOf(value, null);

  if (ArrayIsArray(value)) {
    setOwnProperty(value, SymbolIterator, safeArrayIterator);
    for (let n = 0; n < value.length; n++)
      detachFromUserPrototypes(value[n]);
    return;
  }

  const keys = ObjectKeys(value);
  for (let n = 0; n < keys.length; n++)
    detachFromUserPrototypes(value[keys[n]]);
}

// Parse wrapped JWK bytes according to WebCrypto's "parse a JWK" procedure.
function parseJwk(data) {
  let key;
  try {
    // WebCrypto parses JWKs in a fresh global. Detach parsed JSON values
    // from user-mutated prototypes before WebIDL dictionary conversion.
    // Wrapped JWKs may be produced outside WebCrypto, so parse using the
    // spec-required UTF-8.
    const json = decodeUTF8(data, false, true);
    const result = JSONParse(json);
    detachFromUserPrototypes(result);
    key = webidl.converters.JsonWebKey(result);
  } catch (err) {
    throw lazyDOMException(
      'Invalid wrapped JWK key',
      { name: 'DataError', cause: err });
  }

  if (!ObjectPrototypeHasOwnProperty(key, 'kty')) {
    throw lazyDOMException(
      'Invalid wrapped JWK key',
      'DataError');
  }

  return key;
}

function aliasKeyFormat(format, alias) {
  return format === alias ? 'raw' : format;
}

function importKeySync(format, keyData, algorithm, extractable, usages) {
  let result;
  switch (algorithm.name) {
    case 'RSASSA-PKCS1-v1_5':
      // Fall through
    case 'RSA-PSS':
      // Fall through
    case 'RSA-OAEP':
      result = require('internal/crypto/rsa')
        .rsaImportKey(
          format,
          keyData,
          algorithm,
          extractable,
          usages);
      break;
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      format = aliasKeyFormat(format, 'raw-public');
      result = require('internal/crypto/ec')
        .ecImportKey(
          format,
          keyData,
          algorithm,
          extractable,
          usages);
      break;
    case 'Ed25519':
      // Fall through
    case 'Ed448':
      // Fall through
    case 'X25519':
      // Fall through
    case 'X448':
      format = aliasKeyFormat(format, 'raw-public');
      result = require('internal/crypto/cfrg')
        .cfrgImportKey(
          format,
          keyData,
          algorithm,
          extractable,
          usages);
      break;
    case 'HMAC':
      // Fall through
    case 'KMAC128':
      // Fall through
    case 'KMAC256':
      result = require('internal/crypto/mac')
        .macImportKey(
          format,
          keyData,
          algorithm,
          extractable,
          usages);
      break;
    case 'AES-CTR':
      // Fall through
    case 'AES-CBC':
      // Fall through
    case 'AES-GCM':
      // Fall through
    case 'AES-KW':
      // Fall through
    case 'AES-OCB':
      result = require('internal/crypto/aes')
        .aesImportKey(
          algorithm,
          format,
          keyData,
          extractable,
          usages);
      break;
    case 'ChaCha20-Poly1305':
      result = require('internal/crypto/chacha20_poly1305')
        .c20pImportKey(
          algorithm,
          format,
          keyData,
          extractable,
          usages);
      break;
    case 'HKDF':
      // Fall through
    case 'PBKDF2':
      format = aliasKeyFormat(format, 'raw-secret');
      result = importGenericSecretKey(
        algorithm,
        format,
        keyData,
        extractable,
        usages);
      break;
    case 'Argon2d':
      // Fall through
    case 'Argon2i':
      // Fall through
    case 'Argon2id':
      if (format === 'raw-secret') {
        result = importGenericSecretKey(
          algorithm,
          format,
          keyData,
          extractable,
          usages);
      }
      break;
    case 'ML-DSA-44':
      // Fall through
    case 'ML-DSA-65':
      // Fall through
    case 'ML-DSA-87':
      result = require('internal/crypto/ml_dsa')
        .mlDsaImportKey(
          format,
          keyData,
          algorithm,
          extractable,
          usages);
      break;
    case 'ML-KEM-512':
      // Fall through
    case 'ML-KEM-768':
      // Fall through
    case 'ML-KEM-1024':
      result = require('internal/crypto/ml_kem')
        .mlKemImportKey(
          format,
          keyData,
          algorithm,
          extractable,
          usages);
      break;
  }

  if (!result) {
    throw lazyDOMException(
      `Unable to import ${algorithm.name} using ${format} format`,
      'NotSupportedError');
  }

  const type = getCryptoKeyType(result);
  if ((type === 'secret' || type === 'private') && getCryptoKeyUsagesMask(result) === 0) {
    throw lazyDOMException(
      `Usages cannot be empty when importing a ${type} key.`,
      'SyntaxError');
  }

  return result;
}

function importKey(
  format,
  keyData,
  algorithm,
  extractable,
  keyUsages) {
  return callSubtleCryptoMethod(importKeyImpl, this, arguments);
}

function importKeyImpl(
  format,
  keyData,
  algorithm,
  extractable,
  keyUsages) {
  const prefix = prepareSubtleMethod(this, 'importKey', arguments.length, 5);
  let i = 0;
  format = convertSubtleArgument(prefix, 'KeyFormat', format, i++);
  const type = format === 'jwk' ? 'JsonWebKey' : 'BufferSource';
  keyData = convertSubtleArgument(prefix, type, keyData, i++);
  algorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', algorithm, i++);
  extractable = convertSubtleArgument(prefix, 'boolean', extractable, i++);
  const usages = convertSubtleArgument(
    prefix, 'sequence<KeyUsage>', keyUsages, i++);

  const normalizedAlgorithm = normalizeAlgorithm(algorithm, 'importKey');

  return FunctionPrototypeCall(
    importKeySync,
    this,
    format,
    keyData,
    normalizedAlgorithm,
    extractable,
    usages,
  );
}

// subtle.wrapKey() is essentially a subtle.exportKey() followed
// by a subtle.encrypt().
function wrapKey(format, key, wrappingKey, wrapAlgorithm) {
  return callSubtleCryptoMethod(wrapKeyImpl, this, arguments);
}

function wrapKeyImpl(format, key, wrappingKey, wrapAlgorithm) {
  const prefix = prepareSubtleMethod(this, 'wrapKey', arguments.length, 4);
  let i = 0;
  format = convertSubtleArgument(prefix, 'KeyFormat', format, i++);
  key = convertSubtleArgument(prefix, 'CryptoKey', key, i++);
  wrappingKey = convertSubtleArgument(prefix, 'CryptoKey', wrappingKey, i++);
  const algorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', wrapAlgorithm, i++);
  let normalizedAlgorithm;
  try {
    normalizedAlgorithm = normalizeAlgorithm(algorithm, 'wrapKey');
  } catch {
    normalizedAlgorithm = normalizeAlgorithm(algorithm, 'encrypt');
  }

  if (normalizedAlgorithm.name !== getCryptoKeyAlgorithm(wrappingKey).name)
    throw lazyDOMException('Key algorithm mismatch', 'InvalidAccessError');

  if (!hasCryptoKeyUsage(wrappingKey, 'wrapKey'))
    throw lazyDOMException(
      'Unable to use this key to wrapKey', 'InvalidAccessError');

  const exportedKey = exportKeySync(format, key);
  let bytes = exportedKey;

  if (format === 'jwk') {
    // The WebCrypto spec stringifies JWKs in a new global object. Rather
    // than create a new realm here, detach this internally generated JWK from
    // user-mutated prototypes so JSON.stringify cannot read inherited toJSON
    // hooks from the current global.
    detachFromUserPrototypes(exportedKey);
    const json = JSONStringify(exportedKey);
    // As per the NOTE in step 13 https://w3c.github.io/webcrypto/#SubtleCrypto-method-wrapKey
    // we're padding AES-KW wrapped JWK to make sure it is always a multiple of 8 bytes
    // in length
    // The spec then UTF-8 encodes json.
    if (normalizedAlgorithm.name === 'AES-KW' && json.length % 8 !== 0) {
      bytes = encodeUtf8String(
        json + StringPrototypeRepeat(' ', 8 - (json.length % 8)));
    } else {
      bytes = encodeUtf8String(json);
    }
  }

  return cipherOrWrap(
    kWebCryptoCipherEncrypt,
    normalizedAlgorithm,
    wrappingKey,
    bytes);
}

// subtle.unwrapKey() is essentially a subtle.decrypt() followed
// by a subtle.importKey().
function unwrapKey(
  format,
  wrappedKey,
  unwrappingKey,
  unwrapAlgorithm,
  unwrappedKeyAlgorithm,
  extractable,
  keyUsages) {
  return callSubtleCryptoMethod(unwrapKeyImpl, this, arguments);
}

function unwrapKeyImpl(
  format,
  wrappedKey,
  unwrappingKey,
  unwrapAlgorithm,
  unwrappedKeyAlgorithm,
  extractable,
  keyUsages) {
  const prefix = prepareSubtleMethod(this, 'unwrapKey', arguments.length, 7);
  let i = 0;
  format = convertSubtleArgument(prefix, 'KeyFormat', format, i++);
  wrappedKey = convertSubtleArgument(prefix, 'BufferSource', wrappedKey, i++);
  unwrappingKey = convertSubtleArgument(
    prefix, 'CryptoKey', unwrappingKey, i++);
  const algorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', unwrapAlgorithm, i++);
  unwrappedKeyAlgorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', unwrappedKeyAlgorithm, i++);
  extractable = convertSubtleArgument(prefix, 'boolean', extractable, i++);
  const usages = convertSubtleArgument(
    prefix, 'sequence<KeyUsage>', keyUsages, i++);

  let normalizedAlgorithm;
  try {
    normalizedAlgorithm = normalizeAlgorithm(algorithm, 'unwrapKey');
  } catch {
    normalizedAlgorithm = normalizeAlgorithm(algorithm, 'decrypt');
  }

  const normalizedKeyAlgorithm =
    normalizeAlgorithm(unwrappedKeyAlgorithm, 'importKey');

  if (normalizedAlgorithm.name !== getCryptoKeyAlgorithm(unwrappingKey).name)
    throw lazyDOMException('Key algorithm mismatch', 'InvalidAccessError');

  if (!hasCryptoKeyUsage(unwrappingKey, 'unwrapKey'))
    throw lazyDOMException(
      'Unable to use this key to unwrapKey', 'InvalidAccessError');

  const bytes = cipherOrWrap(
    kWebCryptoCipherDecrypt,
    normalizedAlgorithm,
    unwrappingKey,
    wrappedKey);

  return jobPromiseThen(bytes, (bytes) => {
    let keyData = bytes;
    if (format === 'jwk') {
      keyData = parseJwk(bytes);
    }

    return FunctionPrototypeCall(
      importKeySync,
      this,
      format,
      keyData,
      normalizedKeyAlgorithm,
      extractable,
      usages,
    );
  });
}

function signVerify(algorithm, key, data, signature) {
  const operation = signature !== undefined ? 'verify' : 'sign'; // This is also usage
  const normalizedAlgorithm = normalizeAlgorithm(algorithm, operation);

  if (normalizedAlgorithm.name !== getCryptoKeyAlgorithm(key).name)
    throw lazyDOMException('Key algorithm mismatch', 'InvalidAccessError');

  if (!hasCryptoKeyUsage(key, operation))
    throw lazyDOMException(
      `Unable to use this key to ${operation}`, 'InvalidAccessError');

  switch (normalizedAlgorithm.name) {
    case 'RSA-PSS':
      // Fall through
    case 'RSASSA-PKCS1-v1_5':
      return require('internal/crypto/rsa')
        .rsaSignVerify(key, data, normalizedAlgorithm, signature);
    case 'ECDSA':
      return require('internal/crypto/ec')
        .ecdsaSignVerify(key, data, normalizedAlgorithm, signature);
    case 'Ed25519':
      // Fall through
    case 'Ed448':
      // Fall through
      return require('internal/crypto/cfrg')
        .eddsaSignVerify(key, data, normalizedAlgorithm, signature);
    case 'HMAC':
      return require('internal/crypto/mac')
        .hmacSignVerify(key, data, normalizedAlgorithm, signature);
    case 'ML-DSA-44':
      // Fall through
    case 'ML-DSA-65':
      // Fall through
    case 'ML-DSA-87':
      return require('internal/crypto/ml_dsa')
        .mlDsaSignVerify(key, data, normalizedAlgorithm, signature);
    case 'KMAC128':
      // Fall through
    case 'KMAC256':
      return require('internal/crypto/mac')
        .kmacSignVerify(key, data, normalizedAlgorithm, signature);
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }
}

function sign(algorithm, key, data) {
  return callSubtleCryptoMethod(signImpl, this, arguments);
}

function signImpl(algorithm, key, data) {
  const prefix = prepareSubtleMethod(this, 'sign', arguments.length, 3);
  let i = 0;
  algorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', algorithm, i++);
  key = convertSubtleArgument(prefix, 'CryptoKey', key, i++);
  data = convertSubtleArgument(prefix, 'BufferSource', data, i++);

  return signVerify(algorithm, key, data);
}

function verify(algorithm, key, signature, data) {
  return callSubtleCryptoMethod(verifyImpl, this, arguments);
}

function verifyImpl(algorithm, key, signature, data) {
  const prefix = prepareSubtleMethod(this, 'verify', arguments.length, 4);
  let i = 0;
  algorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', algorithm, i++);
  key = convertSubtleArgument(prefix, 'CryptoKey', key, i++);
  signature = convertSubtleArgument(prefix, 'BufferSource', signature, i++);
  data = convertSubtleArgument(prefix, 'BufferSource', data, i++);

  return signVerify(algorithm, key, data, signature);
}

function cipherOrWrap(mode, normalizedAlgorithm, key, data) {
  // While WebCrypto allows for larger input buffer sizes, we limit
  // those to sizes that can fit within uint32_t because of limitations
  // in the OpenSSL API.
  validateMaxBufferLength(data, 'data');

  switch (normalizedAlgorithm.name) {
    case 'RSA-OAEP':
      return require('internal/crypto/rsa')
        .rsaCipher(mode, key, data, normalizedAlgorithm);
    case 'AES-CTR':
      // Fall through
    case 'AES-CBC':
      // Fall through
    case 'AES-GCM':
      // Fall through
    case 'AES-OCB':
      // Fall through
    case 'AES-KW':
      return require('internal/crypto/aes')
        .aesCipher(mode, key, data, normalizedAlgorithm);
    case 'ChaCha20-Poly1305':
      return require('internal/crypto/chacha20_poly1305')
        .c20pCipher(mode, key, data, normalizedAlgorithm);
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }
}

function encrypt(algorithm, key, data) {
  return callSubtleCryptoMethod(encryptImpl, this, arguments);
}

function encryptImpl(algorithm, key, data) {
  const prefix = prepareSubtleMethod(this, 'encrypt', arguments.length, 3);
  let i = 0;
  algorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', algorithm, i++);
  key = convertSubtleArgument(prefix, 'CryptoKey', key, i++);
  data = convertSubtleArgument(prefix, 'BufferSource', data, i++);

  const normalizedAlgorithm = normalizeAlgorithm(algorithm, 'encrypt');

  if (normalizedAlgorithm.name !== getCryptoKeyAlgorithm(key).name)
    throw lazyDOMException('Key algorithm mismatch', 'InvalidAccessError');

  if (!hasCryptoKeyUsage(key, 'encrypt'))
    throw lazyDOMException(
      'Unable to use this key to encrypt', 'InvalidAccessError');

  return cipherOrWrap(
    kWebCryptoCipherEncrypt,
    normalizedAlgorithm,
    key,
    data,
  );
}

function decrypt(algorithm, key, data) {
  return callSubtleCryptoMethod(decryptImpl, this, arguments);
}

function decryptImpl(algorithm, key, data) {
  const prefix = prepareSubtleMethod(this, 'decrypt', arguments.length, 3);
  let i = 0;
  algorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', algorithm, i++);
  key = convertSubtleArgument(prefix, 'CryptoKey', key, i++);
  data = convertSubtleArgument(prefix, 'BufferSource', data, i++);

  const normalizedAlgorithm = normalizeAlgorithm(algorithm, 'decrypt');

  if (normalizedAlgorithm.name !== getCryptoKeyAlgorithm(key).name)
    throw lazyDOMException('Key algorithm mismatch', 'InvalidAccessError');

  if (!hasCryptoKeyUsage(key, 'decrypt'))
    throw lazyDOMException(
      'Unable to use this key to decrypt', 'InvalidAccessError');

  return cipherOrWrap(
    kWebCryptoCipherDecrypt,
    normalizedAlgorithm,
    key,
    data,
  );
}

// Implements https://wicg.github.io/webcrypto-modern-algos/#SubtleCrypto-method-getPublicKey
function getPublicKey(key, keyUsages) {
  return callSubtleCryptoMethod(getPublicKeyImpl, this, arguments);
}

function getPublicKeyImpl(key, keyUsages) {
  emitExperimentalWarning('The getPublicKey Web Crypto API method');
  const prefix = prepareSubtleMethod(this, 'getPublicKey', arguments.length, 2);
  let i = 0;
  key = convertSubtleArgument(prefix, 'CryptoKey', key, i++);
  const usages = convertSubtleArgument(
    prefix, 'sequence<KeyUsage>', keyUsages, i++);

  const type = getCryptoKeyType(key);
  if (type !== 'private')
    throw lazyDOMException('key must be a private key',
                           type === 'secret' ? 'NotSupportedError' : 'InvalidAccessError');

  return toPublicCryptoKey(key, usages);
}

function encapsulateBits(encapsulationAlgorithm, encapsulationKey) {
  return callSubtleCryptoMethod(encapsulateBitsImpl, this, arguments);
}

function encapsulateBitsImpl(encapsulationAlgorithm, encapsulationKey) {
  emitExperimentalWarning('The encapsulateBits Web Crypto API method');
  const prefix = prepareSubtleMethod(
    this, 'encapsulateBits', arguments.length, 2);
  let i = 0;
  encapsulationAlgorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', encapsulationAlgorithm, i++);
  encapsulationKey = convertSubtleArgument(
    prefix, 'CryptoKey', encapsulationKey, i++);

  const normalizedEncapsulationAlgorithm =
    normalizeAlgorithm(encapsulationAlgorithm, 'encapsulate');
  const keyAlgorithm = getCryptoKeyAlgorithm(encapsulationKey);

  if (normalizedEncapsulationAlgorithm.name !== keyAlgorithm.name) {
    throw lazyDOMException(
      'key algorithm mismatch',
      'InvalidAccessError');
  }

  if (!hasCryptoKeyUsage(encapsulationKey, 'encapsulateBits')) {
    throw lazyDOMException(
      'encapsulationKey does not have encapsulateBits usage',
      'InvalidAccessError');
  }

  switch (keyAlgorithm.name) {
    case 'ML-KEM-512':
    case 'ML-KEM-768':
    case 'ML-KEM-1024':
      return require('internal/crypto/ml_kem')
        .mlKemEncapsulate(encapsulationKey);
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }
}

function encapsulateKey(
  encapsulationAlgorithm,
  encapsulationKey,
  sharedKeyAlgorithm,
  extractable,
  keyUsages) {
  return callSubtleCryptoMethod(encapsulateKeyImpl, this, arguments);
}

function encapsulateKeyImpl(
  encapsulationAlgorithm,
  encapsulationKey,
  sharedKeyAlgorithm,
  extractable,
  keyUsages) {
  emitExperimentalWarning('The encapsulateKey Web Crypto API method');
  const prefix = prepareSubtleMethod(
    this, 'encapsulateKey', arguments.length, 5);
  let i = 0;
  encapsulationAlgorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', encapsulationAlgorithm, i++);
  encapsulationKey = convertSubtleArgument(
    prefix, 'CryptoKey', encapsulationKey, i++);
  sharedKeyAlgorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', sharedKeyAlgorithm, i++);
  extractable = convertSubtleArgument(prefix, 'boolean', extractable, i++);
  const usages = convertSubtleArgument(
    prefix, 'sequence<KeyUsage>', keyUsages, i++);

  const normalizedEncapsulationAlgorithm =
    normalizeAlgorithm(encapsulationAlgorithm, 'encapsulate');
  const normalizedSharedKeyAlgorithm =
    normalizeAlgorithm(sharedKeyAlgorithm, 'importKey');
  const keyAlgorithm = getCryptoKeyAlgorithm(encapsulationKey);

  if (normalizedEncapsulationAlgorithm.name !== keyAlgorithm.name) {
    throw lazyDOMException(
      'key algorithm mismatch',
      'InvalidAccessError');
  }

  if (!hasCryptoKeyUsage(encapsulationKey, 'encapsulateKey')) {
    throw lazyDOMException(
      'encapsulationKey does not have encapsulateKey usage',
      'InvalidAccessError');
  }

  let encapsulatedBits;
  switch (keyAlgorithm.name) {
    case 'ML-KEM-512':
    case 'ML-KEM-768':
    case 'ML-KEM-1024':
      encapsulatedBits = require('internal/crypto/ml_kem')
        .mlKemEncapsulate(encapsulationKey);
      break;
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }

  return jobPromiseThen(encapsulatedBits, (encapsulatedBits) => {
    const sharedKey = FunctionPrototypeCall(
      importKeySync,
      this,
      'raw-secret',
      encapsulatedBits.sharedKey,
      normalizedSharedKeyAlgorithm,
      extractable,
      usages,
    );

    return {
      ciphertext: encapsulatedBits.ciphertext,
      sharedKey,
    };
  });
}

function decapsulateBits(decapsulationAlgorithm, decapsulationKey, ciphertext) {
  return callSubtleCryptoMethod(decapsulateBitsImpl, this, arguments);
}

function decapsulateBitsImpl(decapsulationAlgorithm, decapsulationKey, ciphertext) {
  emitExperimentalWarning('The decapsulateBits Web Crypto API method');
  const prefix = prepareSubtleMethod(
    this, 'decapsulateBits', arguments.length, 3);
  let i = 0;
  decapsulationAlgorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', decapsulationAlgorithm, i++);
  decapsulationKey = convertSubtleArgument(
    prefix, 'CryptoKey', decapsulationKey, i++);
  ciphertext = convertSubtleArgument(prefix, 'BufferSource', ciphertext, i++);

  const normalizedDecapsulationAlgorithm =
    normalizeAlgorithm(decapsulationAlgorithm, 'decapsulate');
  const keyAlgorithm = getCryptoKeyAlgorithm(decapsulationKey);

  if (normalizedDecapsulationAlgorithm.name !== keyAlgorithm.name) {
    throw lazyDOMException(
      'key algorithm mismatch',
      'InvalidAccessError');
  }

  if (!hasCryptoKeyUsage(decapsulationKey, 'decapsulateBits')) {
    throw lazyDOMException(
      'decapsulationKey does not have decapsulateBits usage',
      'InvalidAccessError');
  }

  switch (keyAlgorithm.name) {
    case 'ML-KEM-512':
    case 'ML-KEM-768':
    case 'ML-KEM-1024':
      return require('internal/crypto/ml_kem')
        .mlKemDecapsulate(decapsulationKey, ciphertext);
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }
}

function decapsulateKey(
  decapsulationAlgorithm,
  decapsulationKey,
  ciphertext,
  sharedKeyAlgorithm,
  extractable,
  keyUsages) {
  return callSubtleCryptoMethod(decapsulateKeyImpl, this, arguments);
}

function decapsulateKeyImpl(
  decapsulationAlgorithm,
  decapsulationKey,
  ciphertext,
  sharedKeyAlgorithm,
  extractable,
  keyUsages) {
  emitExperimentalWarning('The decapsulateKey Web Crypto API method');
  const prefix = prepareSubtleMethod(
    this, 'decapsulateKey', arguments.length, 6);
  let i = 0;
  decapsulationAlgorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', decapsulationAlgorithm, i++);
  decapsulationKey = convertSubtleArgument(
    prefix, 'CryptoKey', decapsulationKey, i++);
  ciphertext = convertSubtleArgument(prefix, 'BufferSource', ciphertext, i++);
  sharedKeyAlgorithm = convertSubtleArgument(
    prefix, 'AlgorithmIdentifier', sharedKeyAlgorithm, i++);
  extractable = convertSubtleArgument(prefix, 'boolean', extractable, i++);
  const usages = convertSubtleArgument(
    prefix, 'sequence<KeyUsage>', keyUsages, i++);

  const normalizedDecapsulationAlgorithm =
    normalizeAlgorithm(decapsulationAlgorithm, 'decapsulate');
  const normalizedSharedKeyAlgorithm =
    normalizeAlgorithm(sharedKeyAlgorithm, 'importKey');
  const keyAlgorithm = getCryptoKeyAlgorithm(decapsulationKey);

  if (normalizedDecapsulationAlgorithm.name !== keyAlgorithm.name) {
    throw lazyDOMException(
      'key algorithm mismatch',
      'InvalidAccessError');
  }

  if (!hasCryptoKeyUsage(decapsulationKey, 'decapsulateKey')) {
    throw lazyDOMException(
      'decapsulationKey does not have decapsulateKey usage',
      'InvalidAccessError');
  }

  let decapsulatedBits;
  switch (keyAlgorithm.name) {
    case 'ML-KEM-512':
    case 'ML-KEM-768':
    case 'ML-KEM-1024':
      decapsulatedBits = require('internal/crypto/ml_kem')
        .mlKemDecapsulate(decapsulationKey, ciphertext);
      break;
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }

  return jobPromiseThen(decapsulatedBits, (decapsulatedBits) => FunctionPrototypeCall(
    importKeySync,
    this,
    'raw-secret',
    decapsulatedBits,
    normalizedSharedKeyAlgorithm,
    extractable,
    usages,
  ));
}

// The SubtleCrypto and Crypto classes are defined as part of the
// Web Crypto API standard: https://www.w3.org/TR/WebCryptoAPI/

class SubtleCrypto {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }

  // Implements https://wicg.github.io/webcrypto-modern-algos/#SubtleCrypto-method-supports
  static supports(operation, algorithm, lengthOrAdditionalAlgorithm = null) {
    emitExperimentalWarning('The supports Web Crypto API method');
    if (this !== SubtleCrypto) throw new ERR_INVALID_THIS('SubtleCrypto constructor');
    webidl ??= require('internal/crypto/webidl');
    const prefix = "Failed to execute 'supports' on 'SubtleCrypto'";
    webidl.requiredArguments(arguments.length, 2, { prefix });

    operation = webidl.converters.DOMString(operation, {
      prefix,
      context: '1st argument',
    });
    algorithm = webidl.converters.AlgorithmIdentifier(algorithm, {
      prefix,
      context: '2nd argument',
    });

    switch (operation) {
      case 'decapsulateBits':
      case 'decapsulateKey':
      case 'decrypt':
      case 'deriveBits':
      case 'deriveKey':
      case 'digest':
      case 'encapsulateBits':
      case 'encapsulateKey':
      case 'encrypt':
      case 'exportKey':
      case 'generateKey':
      case 'getPublicKey':
      case 'importKey':
      case 'sign':
      case 'unwrapKey':
      case 'verify':
      case 'wrapKey':
        break;
      default:
        return false;
    }

    let length;
    let additionalAlgorithm;
    if (operation === 'deriveKey') {
      additionalAlgorithm = webidl.converters.AlgorithmIdentifier(
        lengthOrAdditionalAlgorithm,
        {
          prefix,
          context: '3rd argument',
        },
      );

      if (!check('importKey', additionalAlgorithm)) {
        return false;
      }

      try {
        length = getKeyLength(normalizeAlgorithm(additionalAlgorithm, 'get key length'));
      } catch {
        return false;
      }

      operation = 'deriveBits';
    } else if (operation === 'wrapKey') {
      additionalAlgorithm = webidl.converters.AlgorithmIdentifier(
        lengthOrAdditionalAlgorithm,
        {
          prefix,
          context: '3rd argument',
        },
      );

      if (!check('exportKey', additionalAlgorithm)) {
        return false;
      }
    } else if (operation === 'unwrapKey') {
      additionalAlgorithm = webidl.converters.AlgorithmIdentifier(
        lengthOrAdditionalAlgorithm,
        {
          prefix,
          context: '3rd argument',
        },
      );

      if (!check('importKey', additionalAlgorithm)) {
        return false;
      }
    } else if (operation === 'deriveBits') {
      length = lengthOrAdditionalAlgorithm;
      if (length !== null) {
        length = webidl.converters['unsigned long'](length, {
          prefix,
          context: '3rd argument',
        });
      }
    } else if (operation === 'getPublicKey') {
      let normalizedAlgorithm;
      try {
        normalizedAlgorithm = normalizeAlgorithm(algorithm, 'exportKey');
      } catch {
        return false;
      }

      switch (StringPrototypeSlice(normalizedAlgorithm.name, 0, 2)) {
        case 'ML': // ML-DSA-*, ML-KEM-*
        case 'SL': // SLH-DSA-*
        case 'RS': // RSA-OAEP, RSA-PSS, RSASSA-PKCS1-v1_5
        case 'EC': // ECDSA, ECDH
        case 'Ed': // Ed*
        case 'X2': // X25519
        case 'X4': // X448
          return true;
        default:
          return false;
      }
    } else if (operation === 'encapsulateKey' || operation === 'decapsulateKey') {
      additionalAlgorithm = webidl.converters.AlgorithmIdentifier(
        lengthOrAdditionalAlgorithm,
        {
          prefix,
          context: '3rd argument',
        },
      );

      let normalizedAdditionalAlgorithm;
      try {
        normalizedAdditionalAlgorithm = normalizeAlgorithm(additionalAlgorithm, 'importKey');
      } catch {
        return false;
      }

      switch (normalizedAdditionalAlgorithm.name) {
        case 'AES-OCB':
        case 'AES-KW':
        case 'AES-GCM':
        case 'AES-CTR':
        case 'AES-CBC':
        case 'ChaCha20-Poly1305':
        case 'HKDF':
        case 'PBKDF2':
        case 'Argon2i':
        case 'Argon2d':
        case 'Argon2id':
          break;
        case 'HMAC':
        case 'KMAC128':
        case 'KMAC256':
          if (normalizedAdditionalAlgorithm.length === undefined ||
              numBitsToBytes(normalizedAdditionalAlgorithm.length) === 32) {
            break;
          }
          return false;
        default:
          return false;
      }
    }

    try {
      return check(operation, algorithm, length);
    } catch {
      return false;
    }
  }
}

function check(op, alg, length) {
  if (op === 'encapsulateBits' || op === 'encapsulateKey') {
    op = 'encapsulate';
  }

  if (op === 'decapsulateBits' || op === 'decapsulateKey') {
    op = 'decapsulate';
  }

  let normalizedAlgorithm;
  try {
    normalizedAlgorithm = normalizeAlgorithm(alg, op);
  } catch {
    if (op === 'wrapKey') {
      return check('encrypt', alg);
    }

    if (op === 'unwrapKey') {
      return check('decrypt', alg);
    }

    return false;
  }

  switch (op) {
    case 'decapsulate':
    case 'decrypt':
    case 'digest': {
      if ((normalizedAlgorithm.name === 'cSHAKE128' ||
           normalizedAlgorithm.name === 'cSHAKE256') &&
          (normalizedAlgorithm.functionName?.byteLength ||
           normalizedAlgorithm.customization?.byteLength)) {
        return CShakeJob !== undefined;
      }
      return true;
    }
    case 'encapsulate':
    case 'encrypt':
    case 'exportKey':
    case 'importKey':
    case 'sign':
    case 'unwrapKey':
    case 'verify':
    case 'wrapKey':
      return true;
    case 'deriveBits': {
      if (normalizedAlgorithm.name === 'HKDF') {
        require('internal/crypto/hkdf').validateHkdfDeriveBitsLength(length);
      }

      if (normalizedAlgorithm.name === 'PBKDF2') {
        require('internal/crypto/pbkdf2').validatePbkdf2DeriveBitsLength(length);
      }

      if (StringPrototypeStartsWith(normalizedAlgorithm.name, 'Argon2')) {
        require('internal/crypto/argon2').validateArgon2DeriveBitsLength(length);
      }

      if (normalizedAlgorithm.name === 'X25519' && length > 256) {
        return false;
      }

      if (normalizedAlgorithm.name === 'X448' && length > 448) {
        return false;
      }

      if (normalizedAlgorithm.name === 'ECDH') {
        const namedCurve = getCryptoKeyAlgorithm(normalizedAlgorithm.public).namedCurve;
        const maxLength = {
          '__proto__': null,
          'P-256': 256,
          'P-384': 384,
          'P-521': 528,
        }[namedCurve];

        if (length > maxLength) {
          return false;
        }
      }

      return true;
    }
    case 'generateKey': {
      if (
        normalizedAlgorithm.name === 'HMAC' &&
        normalizedAlgorithm.length === undefined &&
        StringPrototypeStartsWith(normalizedAlgorithm.hash.name, 'SHA3-')
      ) {
        return false;
      }

      return true;
    }
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }
}

const subtle = ReflectConstruct(function() {}, [], SubtleCrypto);

class Crypto {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }

  get subtle() {
    if (this !== crypto) throw new ERR_INVALID_THIS('Crypto');
    return subtle;
  }
}
const crypto = ReflectConstruct(function() {}, [], Crypto);

function getRandomValues(array) {
  if (this !== crypto) throw new ERR_INVALID_THIS('Crypto');

  webidl ??= require('internal/crypto/webidl');
  const prefix = "Failed to execute 'getRandomValues' on 'Crypto'";
  webidl.requiredArguments(arguments.length, 1, { prefix });

  return ReflectApply(_getRandomValues, this, arguments);
}

ObjectDefineProperties(
  Crypto.prototype, {
    [SymbolToStringTag]: {
      __proto__: null,
      enumerable: false,
      configurable: true,
      writable: false,
      value: 'Crypto',
    },
    subtle: kEnumerableProperty,
    getRandomValues: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: getRandomValues,
    },
    randomUUID: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: randomUUID,
    },
  });

ObjectDefineProperties(
  SubtleCrypto.prototype, {
    [SymbolToStringTag]: {
      __proto__: null,
      enumerable: false,
      configurable: true,
      writable: false,
      value: 'SubtleCrypto',
    },
    encrypt: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: encrypt,
    },
    decrypt: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: decrypt,
    },
    sign: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: sign,
    },
    verify: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: verify,
    },
    digest: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: digest,
    },
    generateKey: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: generateKey,
    },
    deriveKey: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: deriveKey,
    },
    deriveBits: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: deriveBits,
    },
    importKey: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: importKey,
    },
    exportKey: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: exportKey,
    },
    wrapKey: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: wrapKey,
    },
    unwrapKey: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: unwrapKey,
    },
    getPublicKey: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: getPublicKey,
    },
    encapsulateBits: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: encapsulateBits,
    },
    encapsulateKey: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: encapsulateKey,
    },
    decapsulateBits: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: decapsulateBits,
    },
    decapsulateKey: {
      __proto__: null,
      enumerable: true,
      configurable: true,
      writable: true,
      value: decapsulateKey,
    },
  });

ObjectDefineProperties(SubtleCrypto, {
  supports: kEnumerableProperty,
});

module.exports = {
  Crypto,
  CryptoKey,
  SubtleCrypto,
  crypto,
};
