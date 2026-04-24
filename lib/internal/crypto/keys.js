'use strict';

const {
  ArrayPrototypeSlice,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectSetPrototypeOf,
  SafeSet,
  Symbol,
  SymbolToStringTag,
  Uint8Array,
} = primordials;

const {
  KeyObjectHandle,
  createNativeKeyObjectClass,
  createCryptoKeyClass,
  // eslint-disable-next-line no-restricted-syntax -- intended here
  getCryptoKeySlots: nativeGetCryptoKeySlots,
  kKeyTypeSecret,
  kKeyTypePublic,
  kKeyTypePrivate,
  kKeyFormatPEM,
  kKeyFormatDER,
  kKeyFormatJWK,
  kKeyFormatRawPublic,
  kKeyFormatRawPrivate,
  kKeyFormatRawSeed,
  kKeyEncodingPKCS1,
  kKeyEncodingPKCS8,
  kKeyEncodingSPKI,
  kKeyEncodingSEC1,
} = internalBinding('crypto');

const {
  crypto: {
    POINT_CONVERSION_COMPRESSED,
    POINT_CONVERSION_UNCOMPRESSED,
  },
} = internalBinding('constants');

const {
  validateObject,
  validateOneOf,
  validateString,
} = require('internal/validators');

const {
  codes: {
    ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS,
    ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE,
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  kHandle,
  getArrayBufferOrView,
  bigIntArrayToUnsignedBigInt,
  normalizeAlgorithm,
  hasAnyNotIn,
  getUsagesMask,
  getUsagesFromMask,
  hasUsage,
} = require('internal/crypto/util');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');

const {
  customInspectSymbol: kInspect,
  getDeprecationWarningEmitter,
  kEnumerableProperty,
  lazyDOMException,
} = require('internal/util');

const { inspect } = require('internal/util/inspect');

// Module-local symbol used by KeyObject to store its `type` string
// ("secret"/"public"/"private"). It is also used by `isKeyObject` to
// distinguish KeyObject instances from other types. CryptoKey no longer
// uses any module-local symbol slots - its state lives in C++ internal
// fields on `NativeCryptoKey`.
const kKeyType = Symbol('kKeyType');

const emitDEP0203 = getDeprecationWarningEmitter(
  'DEP0203',
  'Passing a CryptoKey to node:crypto functions is deprecated.',
);

const maybeEmitDEP0204 = getDeprecationWarningEmitter(
  'DEP0204',
  'Passing a non-extractable CryptoKey to KeyObject.from() is deprecated.',
  undefined,
  false,
  (key) => !getCryptoKeyExtractable(key),
);

// Key input contexts.
const kConsumePublic = 0;
const kConsumePrivate = 1;
const kCreatePublic = 2;
const kCreatePrivate = 3;

const encodingNames = [];
for (const m of [[kKeyEncodingPKCS1, 'pkcs1'], [kKeyEncodingPKCS8, 'pkcs8'],
                 [kKeyEncodingSPKI, 'spki'], [kKeyEncodingSEC1, 'sec1']])
  encodingNames[m[0]] = m[1];

// Creating the KeyObject class is a little complicated due to inheritance
// and the fact that KeyObjects should be transferable between threads,
// which requires the KeyObject base class to be implemented in C++.
// The creation requires a callback to make sure that the NativeKeyObject
// base class cannot exist without the other KeyObject implementations.
const {
  0: KeyObject,
  1: SecretKeyObject,
  2: PublicKeyObject,
  3: PrivateKeyObject,
} = createNativeKeyObjectClass((NativeKeyObject) => {
  // Publicly visible KeyObject class.
  class KeyObject extends NativeKeyObject {
    constructor(type, handle) {
      if (type !== 'secret' && type !== 'public' && type !== 'private')
        throw new ERR_INVALID_ARG_VALUE('type', type);
      if (typeof handle !== 'object' || !(handle instanceof KeyObjectHandle))
        throw new ERR_INVALID_ARG_TYPE('handle', 'object', handle);

      super(handle);

      this[kKeyType] = type;

      ObjectDefineProperty(this, kHandle, {
        __proto__: null,
        value: handle,
        enumerable: false,
        configurable: false,
        writable: false,
      });
    }

    get type() {
      return this[kKeyType];
    }

    static from(key) {
      if (!isCryptoKey(key))
        throw new ERR_INVALID_ARG_TYPE('key', 'CryptoKey', key);
      maybeEmitDEP0204(key);
      const handle = getCryptoKeyHandle(key);
      switch (getCryptoKeyType(key)) {
        /* eslint-disable no-use-before-define */
        case 'secret': return new SecretKeyObject(handle);
        case 'public': return new PublicKeyObject(handle);
        case 'private': return new PrivateKeyObject(handle);
        /* eslint-enable no-use-before-define */
      }
    }

    equals(otherKeyObject) {
      if (!isKeyObject(otherKeyObject)) {
        throw new ERR_INVALID_ARG_TYPE(
          'otherKeyObject', 'KeyObject', otherKeyObject);
      }

      return otherKeyObject.type === this.type &&
        this[kHandle].equals(otherKeyObject[kHandle]);
    }
  }

  ObjectDefineProperties(KeyObject.prototype, {
    [SymbolToStringTag]: {
      __proto__: null,
      configurable: true,
      value: 'KeyObject',
    },
  });

  let webidl;

  class SecretKeyObject extends KeyObject {
    constructor(handle) {
      super('secret', handle);
    }

    get symmetricKeySize() {
      return this[kHandle].getSymmetricKeySize();
    }

    export(options) {
      if (options !== undefined) {
        validateObject(options, 'options');
        validateOneOf(
          options.format, 'options.format', [undefined, 'buffer', 'jwk']);
        if (options.format === 'jwk') {
          return this[kHandle].exportJwk({}, false);
        }
      }
      return this[kHandle].export();
    }

    toCryptoKey(algorithm, extractable, keyUsages) {
      webidl ??= require('internal/crypto/webidl');
      algorithm = normalizeAlgorithm(webidl.converters.AlgorithmIdentifier(algorithm), 'importKey');
      extractable = webidl.converters.boolean(extractable);
      keyUsages = webidl.converters['sequence<KeyUsage>'](keyUsages);

      let result;
      switch (algorithm.name) {
        case 'HMAC':
          // Fall through
        case 'KMAC128':
          // Fall through
        case 'KMAC256':
          result = require('internal/crypto/mac')
            .macImportKey('KeyObject', this, algorithm, extractable, keyUsages);
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
            .aesImportKey(algorithm, 'KeyObject', this, extractable, keyUsages);
          break;
        case 'ChaCha20-Poly1305':
          result = require('internal/crypto/chacha20_poly1305')
            .c20pImportKey(algorithm, 'KeyObject', this, extractable, keyUsages);
          break;
        case 'HKDF':
          // Fall through
        case 'PBKDF2':
          // Fall through
        case 'Argon2d':
          // Fall through
        case 'Argon2i':
          // Fall through
        case 'Argon2id':
          result = importGenericSecretKey(
            algorithm,
            'KeyObject',
            this,
            extractable,
            keyUsages);
          break;
        default:
          throw lazyDOMException('Unrecognized algorithm name', 'NotSupportedError');
      }

      if (getCryptoKeyUsagesMask(result) === 0) {
        throw lazyDOMException(
          `Usages cannot be empty when importing a ${getCryptoKeyType(result)} key.`,
          'SyntaxError');
      }

      return result;
    }
  }

  const kAsymmetricKeyType = Symbol('kAsymmetricKeyType');
  const kAsymmetricKeyDetails = Symbol('kAsymmetricKeyDetails');

  function normalizeKeyDetails(details = {}) {
    if (details.publicExponent !== undefined) {
      return {
        ...details,
        publicExponent:
          bigIntArrayToUnsignedBigInt(new Uint8Array(details.publicExponent)),
      };
    }
    return details;
  }

  class AsymmetricKeyObject extends KeyObject {
    get asymmetricKeyType() {
      return this[kAsymmetricKeyType] ||= this[kHandle].getAsymmetricKeyType();
    }

    get asymmetricKeyDetails() {
      switch (this.asymmetricKeyType) {
        case 'rsa':
        case 'rsa-pss':
        case 'dsa':
        case 'ec':
          return this[kAsymmetricKeyDetails] ||= normalizeKeyDetails(
            this[kHandle].keyDetail({}),
          );
        default:
          return {};
      }
    }

    toCryptoKey(algorithm, extractable, keyUsages) {
      webidl ??= require('internal/crypto/webidl');
      algorithm = normalizeAlgorithm(webidl.converters.AlgorithmIdentifier(algorithm), 'importKey');
      extractable = webidl.converters.boolean(extractable);
      keyUsages = webidl.converters['sequence<KeyUsage>'](keyUsages);

      let result;
      switch (algorithm.name) {
        case 'RSASSA-PKCS1-v1_5':
          // Fall through
        case 'RSA-PSS':
          // Fall through
        case 'RSA-OAEP':
          result = require('internal/crypto/rsa')
            .rsaImportKey('KeyObject', this, algorithm, extractable, keyUsages);
          break;
        case 'ECDSA':
          // Fall through
        case 'ECDH':
          result = require('internal/crypto/ec')
            .ecImportKey('KeyObject', this, algorithm, extractable, keyUsages);
          break;
        case 'Ed25519':
          // Fall through
        case 'Ed448':
          // Fall through
        case 'X25519':
          // Fall through
        case 'X448':
          result = require('internal/crypto/cfrg')
            .cfrgImportKey('KeyObject', this, algorithm, extractable, keyUsages);
          break;
        case 'ML-DSA-44':
          // Fall through
        case 'ML-DSA-65':
          // Fall through
        case 'ML-DSA-87':
          result = require('internal/crypto/ml_dsa')
            .mlDsaImportKey('KeyObject', this, algorithm, extractable, keyUsages);
          break;
        case 'ML-KEM-512':
          // Fall through
        case 'ML-KEM-768':
          // Fall through
        case 'ML-KEM-1024':
          result = require('internal/crypto/ml_kem')
            .mlKemImportKey('KeyObject', this, algorithm, extractable, keyUsages);
          break;
        default:
          throw lazyDOMException('Unrecognized algorithm name', 'NotSupportedError');
      }

      const resultType = getCryptoKeyType(result);
      if (resultType === 'private' && getCryptoKeyUsagesMask(result) === 0) {
        throw lazyDOMException(
          `Usages cannot be empty when importing a ${resultType} key.`,
          'SyntaxError');
      }

      return result;
    }
  }

  class PublicKeyObject extends AsymmetricKeyObject {
    constructor(handle) {
      super('public', handle);
    }

    export(options) {
      switch (options?.format) {
        case 'jwk':
          return this[kHandle].exportJwk({}, false);
        case 'raw-public': {
          if (this.asymmetricKeyType === 'ec') {
            const { type = 'uncompressed' } = options;
            validateOneOf(type, 'options.type', ['compressed', 'uncompressed']);
            const form = type === 'compressed' ?
              POINT_CONVERSION_COMPRESSED : POINT_CONVERSION_UNCOMPRESSED;
            return this[kHandle].exportECPublicRaw(form);
          }
          return this[kHandle].rawPublicKey();
        }
        default: {
          const {
            format,
            type,
          } = parsePublicKeyEncoding(options, this.asymmetricKeyType);
          return this[kHandle].export(format, type);
        }
      }
    }
  }

  class PrivateKeyObject extends AsymmetricKeyObject {
    constructor(handle) {
      super('private', handle);
    }

    export(options) {
      if (options?.passphrase !== undefined &&
          options.format !== 'pem' && options.format !== 'der') {
        throw new ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(
          options.format, 'does not support encryption');
      }
      switch (options?.format) {
        case 'jwk':
          return this[kHandle].exportJwk({}, false);
        case 'raw-private': {
          if (this.asymmetricKeyType === 'ec') {
            return this[kHandle].exportECPrivateRaw();
          }
          return this[kHandle].rawPrivateKey();
        }
        case 'raw-seed':
          return this[kHandle].rawSeed();
        default: {
          const {
            format,
            type,
            cipher,
            passphrase,
          } = parsePrivateKeyEncoding(options, this.asymmetricKeyType);
          return this[kHandle].export(format, type, cipher, passphrase);
        }
      }
    }
  }

  return [KeyObject, SecretKeyObject, PublicKeyObject, PrivateKeyObject];
});

function parseKeyFormat(formatStr, defaultFormat, optionName) {
  if (formatStr === undefined && defaultFormat !== undefined)
    return defaultFormat;
  else if (formatStr === 'pem')
    return kKeyFormatPEM;
  else if (formatStr === 'der')
    return kKeyFormatDER;
  else if (formatStr === 'jwk')
    return kKeyFormatJWK;
  else if (formatStr === 'raw-public')
    return kKeyFormatRawPublic;
  else if (formatStr === 'raw-private')
    return kKeyFormatRawPrivate;
  else if (formatStr === 'raw-seed')
    return kKeyFormatRawSeed;
  throw new ERR_INVALID_ARG_VALUE(optionName, formatStr);
}

function parseKeyType(typeStr, required, keyType, isPublic, optionName) {
  if (typeStr === undefined && !required) {
    return undefined;
  } else if (typeStr === 'pkcs1') {
    if (keyType !== undefined && keyType !== 'rsa') {
      throw new ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(
        typeStr, 'can only be used for RSA keys');
    }
    return kKeyEncodingPKCS1;
  } else if (typeStr === 'spki' && isPublic !== false) {
    return kKeyEncodingSPKI;
  } else if (typeStr === 'pkcs8' && isPublic !== true) {
    return kKeyEncodingPKCS8;
  } else if (typeStr === 'sec1' && isPublic !== true) {
    if (keyType !== undefined && keyType !== 'ec') {
      throw new ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(
        typeStr, 'can only be used for EC keys');
    }
    return kKeyEncodingSEC1;
  }

  throw new ERR_INVALID_ARG_VALUE(optionName, typeStr);
}

function option(name, prefix) {
  return prefix === undefined ?
    `options.${name}` : `${prefix}.${name}`;
}

function parseKeyFormatAndType(enc, keyType, isPublic, objName) {
  const { format: formatStr, type: typeStr } = enc;

  const isInput = keyType === undefined;
  const format = parseKeyFormat(formatStr,
                                isInput ? kKeyFormatPEM : undefined,
                                option('format', objName));

  if (format === kKeyFormatRawPublic) {
    if (isPublic === false) {
      throw new ERR_INVALID_ARG_VALUE(option('format', objName), 'raw-public');
    }
    let type;
    if (typeStr === undefined || typeStr === 'uncompressed') {
      type = POINT_CONVERSION_UNCOMPRESSED;
    } else if (typeStr === 'compressed') {
      type = POINT_CONVERSION_COMPRESSED;
    } else {
      throw new ERR_INVALID_ARG_VALUE(option('type', objName), typeStr);
    }
    return { format, type };
  }

  if (format === kKeyFormatRawPrivate || format === kKeyFormatRawSeed) {
    if (isPublic === true) {
      throw new ERR_INVALID_ARG_VALUE(
        option('format', objName),
        format === kKeyFormatRawPrivate ? 'raw-private' : 'raw-seed');
    }
    if (typeStr !== undefined) {
      throw new ERR_INVALID_ARG_VALUE(option('type', objName), typeStr);
    }
    return { format };
  }

  const isRequired = (!isInput ||
                      format === kKeyFormatDER) &&
                      format !== kKeyFormatJWK;
  const type = parseKeyType(typeStr,
                            isRequired,
                            keyType,
                            isPublic,
                            option('type', objName));
  return { format, type };
}

function isStringOrBuffer(val) {
  return typeof val === 'string' ||
         isArrayBufferView(val) ||
         isAnyArrayBuffer(val);
}

function parseKeyEncoding(enc, keyType, isPublic, objName) {
  validateObject(enc, 'options');

  const isInput = keyType === undefined;

  const {
    format,
    type,
  } = parseKeyFormatAndType(enc, keyType, isPublic, objName);

  let cipher, passphrase, encoding;
  if (isPublic !== true) {
    ({ cipher, passphrase, encoding } = enc);

    if (format === kKeyFormatRawPrivate || format === kKeyFormatRawSeed) {
      if (cipher != null || passphrase !== undefined) {
        throw new ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(
          'raw format', 'does not support encryption');
      }
      return { format, type };
    }

    if (!isInput) {
      if (cipher != null) {
        if (typeof cipher !== 'string')
          throw new ERR_INVALID_ARG_VALUE(option('cipher', objName), cipher);
        if (format === kKeyFormatDER &&
            (type === kKeyEncodingPKCS1 ||
             type === kKeyEncodingSEC1)) {
          throw new ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(
            encodingNames[type], 'does not support encryption');
        }
      } else if (passphrase !== undefined) {
        throw new ERR_INVALID_ARG_VALUE(option('cipher', objName), cipher);
      }
    }

    if ((isInput && passphrase !== undefined &&
         !isStringOrBuffer(passphrase)) ||
        (!isInput && cipher != null && !isStringOrBuffer(passphrase))) {
      throw new ERR_INVALID_ARG_VALUE(option('passphrase', objName),
                                      passphrase);
    }
  }

  if (passphrase !== undefined)
    passphrase = getArrayBufferOrView(passphrase, 'key.passphrase', encoding);

  return { format, type, cipher, passphrase };
}

// Parses the public key encoding based on an object. keyType must be undefined
// when this is used to parse an input encoding and must be a valid key type if
// used to parse an output encoding.
function parsePublicKeyEncoding(enc, keyType, objName) {
  return parseKeyEncoding(enc, keyType, keyType ? true : undefined, objName);
}

// Parses the private key encoding based on an object. keyType must be undefined
// when this is used to parse an input encoding and must be a valid key type if
// used to parse an output encoding.
function parsePrivateKeyEncoding(enc, keyType, objName) {
  return parseKeyEncoding(enc, keyType, false, objName);
}

function validateAsymmetricKeyType(type, ctx, key) {
  if (ctx === kCreatePrivate) {
    throw new ERR_INVALID_ARG_TYPE(
      'key',
      ['string', 'ArrayBuffer', 'Buffer', 'TypedArray', 'DataView'],
      key,
    );
  }

  if (type !== 'private') {
    if (ctx === kConsumePrivate || ctx === kCreatePublic)
      throw new ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE(type, 'private');
    if (type !== 'public') {
      throw new ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE(type,
                                                   'private or public');
    }
  }
}

function getKeyTypes(allowKeyObject, bufferOnly = false) {
  const types = [
    'ArrayBuffer',
    'Buffer',
    'TypedArray',
    'DataView',
    'string', // Only if bufferOnly == false
    'KeyObject', // Only if allowKeyObject == true && bufferOnly == false
    'CryptoKey', // Only if allowKeyObject == true && bufferOnly == false
  ];
  if (bufferOnly) {
    return ArrayPrototypeSlice(types, 0, 4);
  } else if (!allowKeyObject) {
    return ArrayPrototypeSlice(types, 0, 5);
  }
  return types;
}


function prepareAsymmetricKey(key, ctx, name = 'key') {
  if (isKeyObject(key)) {
    // Best case: A key object, as simple as that.
    validateAsymmetricKeyType(key.type, ctx, key);
    return { data: key[kHandle] };
  }
  if (isCryptoKey(key)) {
    emitDEP0203();
    validateAsymmetricKeyType(getCryptoKeyType(key), ctx, key);
    return { data: getCryptoKeyHandle(key) };
  }
  if (isStringOrBuffer(key)) {
    // Expect PEM by default, mostly for backward compatibility.
    return { format: kKeyFormatPEM, data: getArrayBufferOrView(key, name) };
  }
  if (typeof key === 'object') {
    const { key: data, encoding, format } = key;

    // The 'key' property can be a KeyObject as well to allow specifying
    // additional options such as padding along with the key.
    if (isKeyObject(data)) {
      validateAsymmetricKeyType(data.type, ctx, data);
      return { data: data[kHandle] };
    }
    if (isCryptoKey(data)) {
      emitDEP0203();
      validateAsymmetricKeyType(getCryptoKeyType(data), ctx, data);
      return { data: getCryptoKeyHandle(data) };
    }
    if (format === 'jwk') {
      validateObject(data, `${name}.key`);
      return { data, format: kKeyFormatJWK };
    } else if (format === 'raw-public' || format === 'raw-private' ||
               format === 'raw-seed') {
      if (!isArrayBufferView(data) && !isAnyArrayBuffer(data)) {
        throw new ERR_INVALID_ARG_TYPE(
          `${name}.key`,
          ['ArrayBuffer', 'Buffer', 'TypedArray', 'DataView'],
          data);
      }
      validateString(key.asymmetricKeyType, `${name}.asymmetricKeyType`);
      if (key.asymmetricKeyType === 'ec') {
        validateString(key.namedCurve, `${name}.namedCurve`);
      }
      const rawFormat = parseKeyFormat(format, undefined, `${name}.format`);
      return {
        data: getArrayBufferOrView(data, `${name}.key`),
        format: rawFormat,
        type: key.asymmetricKeyType,
        namedCurve: key.namedCurve ?? null,
      };
    }

    // Either PEM or DER using PKCS#1 or SPKI.
    if (!isStringOrBuffer(data)) {
      throw new ERR_INVALID_ARG_TYPE(
        `${name}.key`,
        getKeyTypes(ctx !== kCreatePrivate),
        data);
    }

    const isPublic =
      (ctx === kConsumePrivate || ctx === kCreatePrivate) ? false : undefined;
    return {
      data: getArrayBufferOrView(data, `${name}.key`, encoding),
      ...parseKeyEncoding(key, undefined, isPublic, name),
    };
  }

  throw new ERR_INVALID_ARG_TYPE(
    name,
    getKeyTypes(ctx !== kCreatePrivate),
    key);
}

function preparePrivateKey(key, name) {
  return prepareAsymmetricKey(key, kConsumePrivate, name);
}

function preparePublicOrPrivateKey(key, name) {
  return prepareAsymmetricKey(key, kConsumePublic, name);
}

function prepareSecretKey(key, encoding, bufferOnly = false) {
  if (!bufferOnly) {
    if (isKeyObject(key)) {
      if (key.type !== 'secret')
        throw new ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE(key.type, 'secret');
      return key[kHandle];
    }
    if (isCryptoKey(key)) {
      emitDEP0203();
      const type = getCryptoKeyType(key);
      if (type !== 'secret')
        throw new ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE(type, 'secret');
      return getCryptoKeyHandle(key);
    }
  }
  if (typeof key !== 'string' &&
      !isArrayBufferView(key) &&
      !isAnyArrayBuffer(key)) {
    throw new ERR_INVALID_ARG_TYPE(
      'key',
      getKeyTypes(!bufferOnly, bufferOnly),
      key);
  }
  return getArrayBufferOrView(key, 'key', encoding);
}

function createSecretKey(key, encoding) {
  key = prepareSecretKey(key, encoding, true);
  const handle = new KeyObjectHandle();
  handle.init(kKeyTypeSecret, key);
  return new SecretKeyObject(handle);
}

function createPublicKey(key) {
  const { format, type, data, passphrase, namedCurve } =
    prepareAsymmetricKey(key, kCreatePublic);
  const handle = new KeyObjectHandle();
  handle.init(kKeyTypePublic, data, format ?? null,
              type ?? null, passphrase ?? null, namedCurve ?? null);
  return new PublicKeyObject(handle);
}

function createPrivateKey(key) {
  const { format, type, data, passphrase, namedCurve } =
    prepareAsymmetricKey(key, kCreatePrivate);
  const handle = new KeyObjectHandle();
  handle.init(kKeyTypePrivate, data, format ?? null,
              type ?? null, passphrase ?? null, namedCurve ?? null);
  return new PrivateKeyObject(handle);
}

function isKeyObject(obj) {
  return obj != null && obj[kKeyType] !== undefined;
}

// CryptoKey is a plain JS class whose prototype's [[Prototype]] is
// Object.prototype, as Web Crypto requires. Instance storage (type enum,
// extractable, algorithm, usages mask, and the KeyObject handle) lives
// on a C++ class, NativeCryptoKey, created by createCryptoKeyClass.
// InternalCryptoKey is the only constructor we expose to internal
// code; it extends NativeCryptoKey to get that storage and then has
// its prototype spliced so the chain visible to user code is:
//   instance -> InternalCryptoKey.prototype
//            -> CryptoKey.prototype
//            -> Object.prototype
//
// All five internal slots are read from C++ in a single call via
// `getCryptoKeySlots`. The resulting array is cached in a private
// class field on `InternalCryptoKey` so that it is invisible to
// reflection (`Object.getOwnPropertySymbols` etc.) and leaves each
// CryptoKey's hidden class pristine. The `getCryptoKey{Type,
// Extractable,Algorithm,Usages,Handle}` helpers index into that
// array and convert native enums/masks back to Web Crypto strings.
// The public `algorithm` getter caches a cloned dictionary and the
// public `usages` getter caches a synthesized array (as Web Crypto
// requires repeat reads to return the same object so a consumer's
// mutation is visible next time).
let getSlots; // Populated by the createCryptoKeyClass callback below.

const kSlotType = 0;
const kSlotExtractable = 1;
const kSlotAlgorithm = 2;
const kSlotUsagesMask = 3;
const kSlotHandle = 4;
const kSlotClonedAlgorithm = 5;
const kSlotClonedUsages = 6;
const kSlotUsages = 7;

function cloneAlgorithm(raw) {
  const cloned = { ...raw };
  if (cloned.hash !== undefined) cloned.hash = { ...cloned.hash };
  if (cloned.publicExponent !== undefined)
    cloned.publicExponent = new Uint8Array(cloned.publicExponent);
  return cloned;
}

const {
  0: CryptoKey,
  1: InternalCryptoKey,
} = createCryptoKeyClass((NativeCryptoKey) => {
  class CryptoKey {
    constructor() {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }

    [kInspect](depth, options) {
      if (depth < 0)
        return this;

      const opts = {
        ...options,
        depth: options.depth == null ? null : options.depth - 1,
      };

      return `CryptoKey ${inspect({
        type: getCryptoKeyType(this),
        extractable: getCryptoKeyExtractable(this),
        algorithm: getCryptoKeyAlgorithm(this),
        usages: getCryptoKeyUsages(this),
      }, opts)}`;
    }

    get type() {
      return getCryptoKeyType(this);
    }

    get extractable() {
      return getCryptoKeyExtractable(this);
    }

    get algorithm() {
      const slots = getSlots(this);
      let cached = slots[kSlotClonedAlgorithm];
      if (cached === undefined) {
        cached = cloneAlgorithm(slots[kSlotAlgorithm]);
        slots[kSlotClonedAlgorithm] = cached;
      }
      return cached;
    }

    get usages() {
      const slots = getSlots(this);
      let cached = slots[kSlotClonedUsages];
      if (cached === undefined) {
        cached = ArrayPrototypeSlice(getCryptoKeyUsagesFromSlots(slots), 0);
        slots[kSlotClonedUsages] = cached;
      }
      return cached;
    }
  }

  class InternalCryptoKey extends NativeCryptoKey {
    #slots;

    static getSlots(key) {
      if (!key || typeof key !== 'object')
        throw new ERR_INVALID_THIS('CryptoKey');
      if (#slots in key) {
        const cached = key.#slots;
        if (cached !== undefined) return cached;
      }
      const slots = nativeGetCryptoKeySlots(key);
      key.#slots = slots;
      return slots;
    }
  }
  getSlots = InternalCryptoKey.getSlots;
  // Hide NativeCryptoKey from user code.
  InternalCryptoKey.prototype.constructor = CryptoKey;
  ObjectSetPrototypeOf(InternalCryptoKey.prototype, CryptoKey.prototype);

  ObjectDefineProperties(CryptoKey.prototype, {
    type: kEnumerableProperty,
    extractable: kEnumerableProperty,
    algorithm: kEnumerableProperty,
    usages: kEnumerableProperty,
    [SymbolToStringTag]: {
      __proto__: null,
      configurable: true,
      value: 'CryptoKey',
    },
  });

  return [CryptoKey, InternalCryptoKey];
});

// The helpers below return a CryptoKey's internal slot value,
// populating the per-instance cache on first access via a single
// native call. The public `type` getter converts the native enum to
// the Web Crypto string. The `usages` helper converts the native usage
// mask to Web Crypto strings. The public `algorithm` / `usages` getters
// on `CryptoKey.prototype` cache their returned objects.

/**
 * Returns the value of a CryptoKey's `[[type]]` internal slot.
 * @param {CryptoKey} key
 * @returns {'secret' | 'public' | 'private'}
 */
function getCryptoKeyType(key) {
  switch (getSlots(key)[kSlotType]) {
    case kKeyTypeSecret: return 'secret';
    case kKeyTypePublic: return 'public';
    case kKeyTypePrivate: return 'private';
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
  }
}

/**
 * Returns the value of a CryptoKey's `[[extractable]]` internal slot.
 * @param {CryptoKey} key
 * @returns {boolean}
 */
function getCryptoKeyExtractable(key) {
  return getSlots(key)[kSlotExtractable];
}

/**
 * Returns the CryptoKey's `[[algorithm]]` internal slot, bypassing the
 * public `algorithm` getter (which returns a cloned copy).
 * @param {CryptoKey} key
 * @returns {object}
 */
function getCryptoKeyAlgorithm(key) {
  return getSlots(key)[kSlotAlgorithm];
}

/**
 * Returns the CryptoKey's native `[[usages]]` mask.
 * @param {CryptoKey} key
 * @returns {number}
 */
function getCryptoKeyUsagesMask(key) {
  return getSlots(key)[kSlotUsagesMask];
}

/**
 * Returns whether a CryptoKey's `[[usages]]` contains `usage`.
 * @param {CryptoKey} key
 * @param {string} usage
 * @returns {boolean}
 */
function hasCryptoKeyUsage(key, usage) {
  return hasUsage(getCryptoKeyUsagesMask(key), usage);
}

/**
 * Returns the CryptoKey's cached canonical usages array for internal
 * consumers, expanding it from the native usage mask on first access.
 * @param {Array} slots
 * @returns {string[]}
 */
function getCryptoKeyUsagesFromSlots(slots) {
  let usages = slots[kSlotUsages];
  if (usages === undefined) {
    usages = getUsagesFromMask(slots[kSlotUsagesMask]);
    slots[kSlotUsages] = usages;
  }
  return usages;
}

/**
 * Returns the CryptoKey's `[[usages]]` internal slot, bypassing the
 * public `usages` getter (which returns a cloned array). The internal
 * array is expanded lazily from the native usage mask.
 * @param {CryptoKey} key
 * @returns {string[]}
 */
function getCryptoKeyUsages(key) {
  return getCryptoKeyUsagesFromSlots(getSlots(key));
}

/**
 * Returns the KeyObjectHandle wrapping the CryptoKey's underlying
 * key material.
 * @param {CryptoKey} key
 * @returns {KeyObjectHandle}
 */
function getCryptoKeyHandle(key) {
  return getSlots(key)[kSlotHandle];
}

function isCryptoKey(obj) {
  if (obj == null || typeof obj !== 'object')
    return false;

  try {
    getSlots(obj);
    return true;
  } catch {
    return false;
  }
}

function importGenericSecretKey(
  algorithm,
  format,
  keyData,
  extractable,
  keyUsages,
) {
  const usagesSet = new SafeSet(keyUsages);
  const { name } = algorithm;
  if (extractable)
    throw lazyDOMException(`${name} keys are not extractable`, 'SyntaxError');

  if (hasAnyNotIn(usagesSet, ['deriveKey', 'deriveBits'])) {
    throw lazyDOMException(
      `Unsupported key usage for a ${name} key`,
      'SyntaxError');
  }

  let handle;
  switch (format) {
    case 'KeyObject': {
      handle = keyData[kHandle];
      break;
    }
    case 'raw-secret':
    case 'raw': {
      handle = new KeyObjectHandle();
      handle.init(kKeyTypeSecret, keyData);
      break;
    }
    default:
      return undefined;
  }

  return new InternalCryptoKey(
    handle,
    { name },
    getUsagesMask(usagesSet),
    false);
}

module.exports = {
  // Public API.
  createSecretKey,
  createPublicKey,
  createPrivateKey,
  KeyObject,
  CryptoKey,
  InternalCryptoKey,

  // These are designed for internal use only and should not be exposed.
  parsePublicKeyEncoding,
  parsePrivateKeyEncoding,
  parseKeyEncoding,
  preparePrivateKey,
  preparePublicOrPrivateKey,
  prepareSecretKey,
  SecretKeyObject,
  PublicKeyObject,
  PrivateKeyObject,
  isKeyObject,
  isCryptoKey,
  getCryptoKeyType,
  getCryptoKeyExtractable,
  getCryptoKeyAlgorithm,
  getCryptoKeyUsages,
  getCryptoKeyUsagesMask,
  hasCryptoKeyUsage,
  getCryptoKeyHandle,
  importGenericSecretKey,
};
