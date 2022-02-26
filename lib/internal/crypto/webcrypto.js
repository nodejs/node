'use strict';

const {
  ArrayPrototypeIncludes,
  JSONParse,
  JSONStringify,
  ObjectDefineProperties,
  ReflectApply,
  SafeSet,
  SymbolToStringTag,
  StringPrototypeRepeat,
} = primordials;

const {
  kWebCryptoKeyFormatRaw,
  kWebCryptoKeyFormatPKCS8,
  kWebCryptoKeyFormatSPKI,
  kWebCryptoCipherEncrypt,
  kWebCryptoCipherDecrypt,
} = internalBinding('crypto');

const {
  validateArray,
  validateBoolean,
  validateObject,
  validateOneOf,
  validateString,
} = require('internal/validators');

const { TextDecoder, TextEncoder } = require('internal/encoding');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_THIS,
  }
} = require('internal/errors');

const {
  CryptoKey,
  InternalCryptoKey,
  createSecretKey,
  isCryptoKey,
  isKeyObject,
} = require('internal/crypto/keys');

const {
  asyncDigest,
} = require('internal/crypto/hash');

const {
  getArrayBufferOrView,
  hasAnyNotIn,
  lazyRequire,
  normalizeAlgorithm,
  normalizeHashName,
  validateMaxBufferLength,
  kExportFormats,
  kHandle,
  kKeyObject,
} = require('internal/crypto/util');

const {
  kEnumerableProperty,
  lazyDOMException,
} = require('internal/util');

const {
  getRandomValues: _getRandomValues,
  randomUUID: _randomUUID,
} = require('internal/crypto/random');

const randomUUID = () => _randomUUID();

async function generateKey(
  algorithm,
  extractable,
  keyUsages) {
  algorithm = normalizeAlgorithm(algorithm);
  validateBoolean(extractable, 'extractable');
  validateArray(keyUsages, 'keyUsages');
  switch (algorithm.name) {
    case 'RSASSA-PKCS1-v1_5':
      // Fall through
    case 'RSA-PSS':
      // Fall through
    case 'RSA-OAEP':
      return lazyRequire('internal/crypto/rsa')
        .rsaKeyGenerate(algorithm, extractable, keyUsages);
    case 'NODE-ED25519':
      // Fall through
    case 'NODE-ED448':
      // Fall through
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      return lazyRequire('internal/crypto/ec')
        .ecGenerateKey(algorithm, extractable, keyUsages);
    case 'HMAC':
      return lazyRequire('internal/crypto/mac')
        .hmacGenerateKey(algorithm, extractable, keyUsages);
    case 'AES-CTR':
      // Fall through
    case 'AES-CBC':
      // Fall through
    case 'AES-GCM':
      // Fall through
    case 'AES-KW':
      return lazyRequire('internal/crypto/aes')
        .aesGenerateKey(algorithm, extractable, keyUsages);

    // Following are Node.js specific extensions. Names must be prefixed
    // with the `NODE-`
    case 'NODE-DSA':
      return lazyRequire('internal/crypto/dsa')
        .dsaGenerateKey(algorithm, extractable, keyUsages);
    case 'NODE-DH':
      return lazyRequire('internal/crypto/diffiehellman')
        .dhGenerateKey(algorithm, extractable, keyUsages);
    default:
      throw lazyDOMException('Unrecognized name.');
  }
}

async function deriveBits(algorithm, baseKey, length) {
  algorithm = normalizeAlgorithm(algorithm);
  if (!isCryptoKey(baseKey))
    throw new ERR_INVALID_ARG_TYPE('baseKey', 'CryptoKey', baseKey);
  if (!ArrayPrototypeIncludes(baseKey.usages, 'deriveBits')) {
    throw lazyDOMException(
      'baseKey does not have deriveBits usage',
      'InvalidAccessError');
  }
  if (baseKey.algorithm.name !== algorithm.name)
    throw lazyDOMException('Key algorithm mismatch', 'InvalidAccessError');
  switch (algorithm.name) {
    case 'ECDH':
      return lazyRequire('internal/crypto/diffiehellman')
        .asyncDeriveBitsECDH(algorithm, baseKey, length);
    case 'HKDF':
      return lazyRequire('internal/crypto/hkdf')
        .hkdfDeriveBits(algorithm, baseKey, length);
    case 'PBKDF2':
      return lazyRequire('internal/crypto/pbkdf2')
        .pbkdf2DeriveBits(algorithm, baseKey, length);
    case 'NODE-SCRYPT':
      return lazyRequire('internal/crypto/scrypt')
        .scryptDeriveBits(algorithm, baseKey, length);
    case 'NODE-DH':
      return lazyRequire('internal/crypto/diffiehellman')
        .asyncDeriveBitsDH(algorithm, baseKey, length);
  }
  throw lazyDOMException('Unrecognized name.');
}

async function deriveKey(
  algorithm,
  baseKey,
  derivedKeyAlgorithm,
  extractable,
  keyUsages) {
  algorithm = normalizeAlgorithm(algorithm);
  derivedKeyAlgorithm = normalizeAlgorithm(derivedKeyAlgorithm);
  if (!isCryptoKey(baseKey))
    throw new ERR_INVALID_ARG_TYPE('baseKey', 'CryptoKey', baseKey);
  if (!ArrayPrototypeIncludes(baseKey.usages, 'deriveKey')) {
    throw lazyDOMException(
      'baseKey does not have deriveKey usage',
      'InvalidAccessError');
  }
  if (baseKey.algorithm.name !== algorithm.name)
    throw lazyDOMException('Key algorithm mismatch', 'InvalidAccessError');
  validateObject(derivedKeyAlgorithm, 'derivedKeyAlgorithm', {
    allowArray: true, allowFunction: true,
  });
  validateBoolean(extractable, 'extractable');
  validateArray(keyUsages, 'keyUsages');

  const { length } = derivedKeyAlgorithm;
  let bits;
  switch (algorithm.name) {
    case 'ECDH':
      bits = await lazyRequire('internal/crypto/diffiehellman')
        .asyncDeriveBitsECDH(algorithm, baseKey, length);
      break;
    case 'HKDF':
      bits = await lazyRequire('internal/crypto/hkdf')
        .hkdfDeriveBits(algorithm, baseKey, length);
      break;
    case 'PBKDF2':
      bits = await lazyRequire('internal/crypto/pbkdf2')
        .pbkdf2DeriveBits(algorithm, baseKey, length);
      break;
    case 'NODE-SCRYPT':
      bits = await lazyRequire('internal/crypto/scrypt')
        .scryptDeriveBits(algorithm, baseKey, length);
      break;
    case 'NODE-DH':
      bits = await lazyRequire('internal/crypto/diffiehellman')
        .asyncDeriveBitsDH(algorithm, baseKey, length);
      break;
    default:
      throw lazyDOMException('Unrecognized name.');
  }

  return importKey('raw', bits, derivedKeyAlgorithm, extractable, keyUsages);
}

async function exportKeySpki(key) {
  switch (key.algorithm.name) {
    case 'RSASSA-PKCS1-v1_5':
      // Fall through
    case 'RSA-PSS':
      // Fall through
    case 'RSA-OAEP':
      if (key.type === 'public') {
        return lazyRequire('internal/crypto/rsa')
          .rsaExportKey(key, kWebCryptoKeyFormatSPKI);
      }
      break;
    case 'NODE-ED25519':
      // Fall through
    case 'NODE-ED448':
      // Fall through
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      if (key.type === 'public') {
        return lazyRequire('internal/crypto/ec')
          .ecExportKey(key, kWebCryptoKeyFormatSPKI);
      }
      break;
    case 'NODE-DSA':
      if (key.type === 'public') {
        return lazyRequire('internal/crypto/dsa')
          .dsaExportKey(key, kWebCryptoKeyFormatSPKI);
      }
      break;
    case 'NODE-DH':
      if (key.type === 'public') {
        return lazyRequire('internal/crypto/diffiehellman')
          .dhExportKey(key, kWebCryptoKeyFormatSPKI);
      }
      break;
  }

  throw lazyDOMException(
    `Unable to export a raw ${key.algorithm.name} ${key.type} key`,
    'InvalidAccessError');
}

async function exportKeyPkcs8(key) {
  switch (key.algorithm.name) {
    case 'RSASSA-PKCS1-v1_5':
      // Fall through
    case 'RSA-PSS':
      // Fall through
    case 'RSA-OAEP':
      if (key.type === 'private') {
        return lazyRequire('internal/crypto/rsa')
          .rsaExportKey(key, kWebCryptoKeyFormatPKCS8);
      }
      break;
    case 'NODE-ED25519':
      // Fall through
    case 'NODE-ED448':
      // Fall through
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      if (key.type === 'private') {
        return lazyRequire('internal/crypto/ec')
          .ecExportKey(key, kWebCryptoKeyFormatPKCS8);
      }
      break;
    case 'NODE-DSA':
      if (key.type === 'private') {
        return lazyRequire('internal/crypto/dsa')
          .dsaExportKey(key, kWebCryptoKeyFormatPKCS8);
      }
      break;
    case 'NODE-DH':
      if (key.type === 'private') {
        return lazyRequire('internal/crypto/diffiehellman')
          .dhExportKey(key, kWebCryptoKeyFormatPKCS8);
      }
      break;
  }

  throw lazyDOMException(
    `Unable to export a pkcs8 ${key.algorithm.name} ${key.type} key`,
    'InvalidAccessError');
}

async function exportKeyRaw(key) {
  switch (key.algorithm.name) {
    case 'NODE-ED25519':
      // Fall through
    case 'NODE-ED448':
      if (key.type === 'public') {
        return lazyRequire('internal/crypto/ec')
          .ecExportKey(key, kWebCryptoKeyFormatRaw);
      }
      break;
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      if (key.type === 'public') {
        return lazyRequire('internal/crypto/ec')
          .ecExportKey(key, kWebCryptoKeyFormatRaw);
      }
      break;
    case 'AES-CTR':
      // Fall through
    case 'AES-CBC':
      // Fall through
    case 'AES-GCM':
      // Fall through
    case 'AES-KW':
      // Fall through
    case 'HMAC':
      return key[kKeyObject].export().buffer;
  }

  throw lazyDOMException(
    `Unable to export a raw ${key.algorithm.name} ${key.type} key`,
    'InvalidAccessError');
}

async function exportKeyJWK(key) {
  const jwk = key[kKeyObject][kHandle].exportJwk({
    key_ops: key.usages,
    ext: key.extractable,
  }, true);
  switch (key.algorithm.name) {
    case 'RSASSA-PKCS1-v1_5':
      jwk.alg = normalizeHashName(
        key.algorithm.hash.name,
        normalizeHashName.kContextJwkRsa);
      return jwk;
    case 'RSA-PSS':
      jwk.alg = normalizeHashName(
        key.algorithm.hash.name,
        normalizeHashName.kContextJwkRsaPss);
      return jwk;
    case 'RSA-OAEP':
      jwk.alg = normalizeHashName(
        key.algorithm.hash.name,
        normalizeHashName.kContextJwkRsaOaep);
      return jwk;
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      jwk.crv ||= key.algorithm.namedCurve;
      return jwk;
    case 'AES-CTR':
      // Fall through
    case 'AES-CBC':
      // Fall through
    case 'AES-GCM':
      // Fall through
    case 'AES-KW':
      jwk.alg = lazyRequire('internal/crypto/aes')
        .getAlgorithmName(key.algorithm.name, key.algorithm.length);
      return jwk;
    case 'HMAC':
      jwk.alg = normalizeHashName(
        key.algorithm.hash.name,
        normalizeHashName.kContextJwkHmac);
      return jwk;
    case 'NODE-DSA':
      jwk.alg = normalizeHashName(
        key.algorithm.hash.name,
        normalizeHashName.kContextJwkDsa);
      return jwk;
    case 'NODE-ED25519':
      // Fall through
    case 'NODE-ED448':
      return jwk;
    default:
      // Fall through
  }

  throw lazyDOMException('Not yet supported', 'NotSupportedError');
}

async function exportKey(format, key) {
  validateString(format, 'format');
  validateOneOf(format, 'format', kExportFormats);
  if (!isCryptoKey(key))
    throw new ERR_INVALID_ARG_TYPE('key', 'CryptoKey', key);

  if (!key.extractable)
    throw lazyDOMException('key is not extractable', 'InvalidAccessException');

  switch (format) {
    case 'node.keyObject': return key[kKeyObject];
    case 'spki': return exportKeySpki(key);
    case 'pkcs8': return exportKeyPkcs8(key);
    case 'jwk': return exportKeyJWK(key);
    case 'raw': return exportKeyRaw(key);
  }
  throw lazyDOMException(
    'Export format is unsupported', 'NotSupportedError');
}

async function importGenericSecretKey(
  { name, length },
  format,
  keyData,
  extractable,
  keyUsages) {
  const usagesSet = new SafeSet(keyUsages);
  if (extractable)
    throw lazyDOMException(`${name} keys are not extractable`, 'SyntaxError');

  if (hasAnyNotIn(usagesSet, ['deriveKey', 'deriveBits'])) {
    throw lazyDOMException(
      `Unsupported key usage for a ${name} key`,
      'SyntaxError');
  }

  switch (format) {
    case 'node.keyObject': {
      if (!isKeyObject(keyData))
        throw new ERR_INVALID_ARG_TYPE('keyData', 'KeyObject', keyData);

      if (keyData.type === 'secret')
        return new InternalCryptoKey(keyData, { name }, keyUsages, extractable);

      break;
    }
    case 'raw': {
      if (hasAnyNotIn(usagesSet, ['deriveKey', 'deriveBits'])) {
        throw lazyDOMException(
          `Unsupported key usage for a ${name} key`,
          'SyntaxError');
      }

      const checkLength = keyData.byteLength * 8;

      if (checkLength === 0 || length === 0)
        throw lazyDOMException('Zero-length key is not supported', 'DataError');

      // The Web Crypto spec allows for key lengths that are not multiples of
      // 8. We don't. Our check here is stricter than that defined by the spec
      // in that we require that algorithm.length match keyData.length * 8 if
      // algorithm.length is specified.
      if (length !== undefined && length !== checkLength) {
        throw lazyDOMException('Invalid key length', 'DataError');
      }

      const keyObject = createSecretKey(keyData);
      return new InternalCryptoKey(keyObject, { name }, keyUsages, false);
    }
  }

  throw lazyDOMException(
    `Unable to import ${name} key with format ${format}`,
    'NotSupportedError');
}

async function importKey(
  format,
  keyData,
  algorithm,
  extractable,
  keyUsages) {
  validateString(format, 'format');
  validateOneOf(format, 'format', kExportFormats);
  if (format !== 'node.keyObject' && format !== 'jwk')
    keyData = getArrayBufferOrView(keyData, 'keyData');
  algorithm = normalizeAlgorithm(algorithm);
  validateBoolean(extractable, 'extractable');
  validateArray(keyUsages, 'keyUsages');
  switch (algorithm.name) {
    case 'RSASSA-PKCS1-v1_5':
      // Fall through
    case 'RSA-PSS':
      // Fall through
    case 'RSA-OAEP':
      return lazyRequire('internal/crypto/rsa')
        .rsaImportKey(format, keyData, algorithm, extractable, keyUsages);
    case 'NODE-ED25519':
      // Fall through
    case 'NODE-ED448':
      // Fall through
    case 'ECDSA':
      // Fall through
    case 'ECDH':
      return lazyRequire('internal/crypto/ec')
        .ecImportKey(format, keyData, algorithm, extractable, keyUsages);
    case 'HMAC':
      return lazyRequire('internal/crypto/mac')
        .hmacImportKey(format, keyData, algorithm, extractable, keyUsages);
    case 'AES-CTR':
      // Fall through
    case 'AES-CBC':
      // Fall through
    case 'AES-GCM':
      // Fall through
    case 'AES-KW':
      return lazyRequire('internal/crypto/aes')
        .aesImportKey(algorithm, format, keyData, extractable, keyUsages);
    case 'HKDF':
      // Fall through
    case 'NODE-SCRYPT':
      // Fall through
    case 'PBKDF2':
      return importGenericSecretKey(
        algorithm,
        format,
        keyData,
        extractable,
        keyUsages);
    case 'NODE-DSA':
      return lazyRequire('internal/crypto/dsa')
        .dsaImportKey(format, keyData, algorithm, extractable, keyUsages);
    case 'NODE-DH':
      return lazyRequire('internal/crypto/diffiehellman')
        .dhImportKey(format, keyData, algorithm, extractable, keyUsages);
  }

  throw lazyDOMException('Unrecognized name.', 'NotSupportedError');
}

// subtle.wrapKey() is essentially a subtle.exportKey() followed
// by a subtle.encrypt().
async function wrapKey(format, key, wrappingKey, algorithm) {
  algorithm = normalizeAlgorithm(algorithm);
  let keyData = await exportKey(format, key);

  if (format === 'jwk') {
    if (keyData == null || typeof keyData !== 'object')
      throw lazyDOMException('Invalid exported JWK key', 'DataError');
    const ec = new TextEncoder();
    const raw = JSONStringify(keyData);
    keyData = ec.encode(raw + StringPrototypeRepeat(' ', 8 - (raw.length % 8)));
  }

  return cipherOrWrap(
    kWebCryptoCipherEncrypt,
    algorithm,
    wrappingKey,
    keyData,
    'wrapKey');
}

// subtle.unwrapKey() is essentially a subtle.decrypt() followed
// by a subtle.importKey().
async function unwrapKey(
  format,
  wrappedKey,
  unwrappingKey,
  unwrapAlgo,
  unwrappedKeyAlgo,
  extractable,
  keyUsages) {
  wrappedKey = getArrayBufferOrView(wrappedKey, 'wrappedKey');

  let keyData = await cipherOrWrap(
    kWebCryptoCipherDecrypt,
    normalizeAlgorithm(unwrapAlgo),
    unwrappingKey,
    wrappedKey,
    'unwrapKey');

  if (format === 'jwk') {
    // The fatal: true option is only supported in builds that have ICU.
    const options = process.versions.icu !== undefined ?
      { fatal: true } : undefined;
    const dec = new TextDecoder('utf-8', options);
    try {
      keyData = JSONParse(dec.decode(keyData));
    } catch {
      throw lazyDOMException('Invalid imported JWK key', 'DataError');
    }
  }

  return importKey(format, keyData, unwrappedKeyAlgo, extractable, keyUsages);
}

function signVerify(algorithm, key, data, signature) {
  algorithm = normalizeAlgorithm(algorithm);
  if (!isCryptoKey(key))
    throw new ERR_INVALID_ARG_TYPE('key', 'CryptoKey', key);
  data = getArrayBufferOrView(data, 'data');
  let usage = 'sign';
  if (signature !== undefined) {
    signature = getArrayBufferOrView(signature, 'signature');
    usage = 'verify';
  }

  if (!ArrayPrototypeIncludes(key.usages, usage) ||
      algorithm.name !== key.algorithm.name) {
    throw lazyDOMException(
      `Unable to use this key to ${usage}`,
      'InvalidAccessError');
  }

  switch (algorithm.name) {
    case 'RSA-PSS':
      // Fall through
    case 'RSASSA-PKCS1-v1_5':
      return lazyRequire('internal/crypto/rsa')
        .rsaSignVerify(key, data, algorithm, signature);
    case 'NODE-ED25519':
      // Fall through
    case 'NODE-ED448':
      // Fall through
    case 'ECDSA':
      return lazyRequire('internal/crypto/ec')
        .ecdsaSignVerify(key, data, algorithm, signature);
    case 'HMAC':
      return lazyRequire('internal/crypto/mac')
        .hmacSignVerify(key, data, algorithm, signature);
    // The following are Node.js specific extensions. They must begin with
    // the `NODE-` prefix
    case 'NODE-DSA':
      return lazyRequire('internal/crypto/dsa')
        .dsaSignVerify(key, data, algorithm, signature);
  }
  throw lazyDOMException('Unrecognized named.', 'NotSupportedError');
}

async function sign(algorithm, key, data) {
  return signVerify(algorithm, key, data);
}

async function verify(algorithm, key, signature, data) {
  return signVerify(algorithm, key, data, signature);
}

async function cipherOrWrap(mode, algorithm, key, data, op) {
  algorithm = normalizeAlgorithm(algorithm);
  // We use a Node.js style error here instead of a DOMException because
  // the WebCrypto spec is not specific what kind of error is to be thrown
  // in this case. Both Firefox and Chrome throw simple TypeErrors here.
  if (!isCryptoKey(key))
    throw new ERR_INVALID_ARG_TYPE('key', 'CryptoKey', key);
  // The key algorithm and cipher algorithm must match, and the
  // key must have the proper usage.
  if (key.algorithm.name !== algorithm.name ||
      !ArrayPrototypeIncludes(key.usages, op)) {
    throw lazyDOMException(
      'The requested operation is not valid for the provided key',
      'InvalidAccessError');
  }

  // For the Web Crypto API, the input data can be any ArrayBuffer,
  // TypedArray, or DataView.
  data = getArrayBufferOrView(data, 'data');

  // While WebCrypto allows for larger input buffer sizes, we limit
  // those to sizes that can fit within uint32_t because of limitations
  // in the OpenSSL API.
  validateMaxBufferLength(data, 'data');

  switch (algorithm.name) {
    case 'RSA-OAEP':
      return lazyRequire('internal/crypto/rsa')
        .rsaCipher(mode, key, data, algorithm);
    case 'AES-CTR':
      // Fall through
    case 'AES-CBC':
      // Fall through
    case 'AES-GCM':
      return lazyRequire('internal/crypto/aes')
        .aesCipher(mode, key, data, algorithm);
    case 'AES-KW':
      if (op === 'wrapKey' || op === 'unwrapKey') {
        return lazyRequire('internal/crypto/aes')
          .aesCipher(mode, key, data, algorithm);
      }
  }
  throw lazyDOMException('Unrecognized name.', 'NotSupportedError');
}

async function encrypt(algorithm, key, data) {
  return cipherOrWrap(kWebCryptoCipherEncrypt, algorithm, key, data, 'encrypt');
}

async function decrypt(algorithm, key, data) {
  return cipherOrWrap(kWebCryptoCipherDecrypt, algorithm, key, data, 'decrypt');
}

// The SubtleCrypto and Crypto classes are defined as part of the
// Web Crypto API standard: https://www.w3.org/TR/WebCryptoAPI/

class SubtleCrypto {}
const subtle = new SubtleCrypto();

class Crypto {
  get subtle() {
    return subtle;
  }
}
const crypto = new Crypto();

function getRandomValues(array) {
  if (!(this instanceof Crypto)) {
    throw new ERR_INVALID_THIS('Crypto');
  }
  return ReflectApply(_getRandomValues, this, arguments);
}

ObjectDefineProperties(
  Crypto.prototype, {
    [SymbolToStringTag]: {
      enumerable: false,
      configurable: true,
      writable: false,
      value: 'Crypto',
    },
    subtle: kEnumerableProperty,
    getRandomValues: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: getRandomValues,
    },
    randomUUID: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: randomUUID,
    },
    CryptoKey: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: CryptoKey,
    }
  });

ObjectDefineProperties(
  SubtleCrypto.prototype, {
    [SymbolToStringTag]: {
      enumerable: false,
      configurable: true,
      writable: false,
      value: 'SubtleCrypto',
    },
    encrypt: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: encrypt,
    },
    decrypt: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: decrypt,
    },
    sign: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: sign,
    },
    verify: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: verify,
    },
    digest: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: asyncDigest,
    },
    generateKey: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: generateKey,
    },
    deriveKey: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: deriveKey,
    },
    deriveBits: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: deriveBits,
    },
    importKey: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: importKey,
    },
    exportKey: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: exportKey,
    },
    wrapKey: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: wrapKey,
    },
    unwrapKey: {
      enumerable: true,
      configurable: true,
      writable: true,
      value: unwrapKey,
    }
  });

module.exports = {
  Crypto,
  CryptoKey,
  SubtleCrypto,
  crypto,
};
