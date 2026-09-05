'use strict';

const {
  ArrayPrototypeSlice,
  BigInt,
  PromiseWithResolvers,
  SafeSet,
  TypedArrayOf,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetLength,
  TypedArrayPrototypeSet,
  TypedArrayPrototypeSubarray,
  Uint8Array,
} = primordials;

const { Buffer } = require('buffer');

const {
  getCryptoKeyExtractable,
  getCryptoKeyAlgorithm,
  getCryptoKeyHandle,
  getCryptoKeySeedData,
  getCryptoKeySecondaryHandle,
  getCryptoKeyType,
  getCryptoKeyUsages,
  InternalCryptoKey,
} = require('internal/crypto/keys');

const {
  DHBitsJob,
  KEMDecapsulateJob,
  KEMEncapsulateJob,
  KeyObjectHandle,
  kCryptoJobWebCrypto,
  kKeyFormatRawPrivate,
  kKeyFormatRawPublic,
  kKeyFormatRawSeed,
  kKeyTypePrivate,
  kKeyTypePublic,
  timingSafeEqual,
} = internalBinding('crypto');

const {
  crypto: {
    POINT_CONVERSION_UNCOMPRESSED,
  },
} = internalBinding('constants');

const {
  getBufferSourceBytes,
  getUsagesMask,
  jobPromise,
  jobPromiseThen,
  resolveWebCryptoResult,
} = require('internal/crypto/util');

const {
  lazyDOMException,
  setOwnProperty,
} = require('internal/util');

const {
  hash,
} = require('internal/crypto/hash');

const {
  randomBytes,
} = require('internal/crypto/random');

const {
  createKeyUsages,
  getKeyPairUsages,
  validateJwk,
  validateKeyUsages,
  validateUsagesNotEmpty,
  verifyAcceptableKeyUse,
} = require('internal/crypto/webcrypto_util');

// Concrete PQ/T Hybrid KEM instances:
// https://www.ietf.org/archive/id/draft-irtf-cfrg-concrete-hybrid-kems-04.html#section-4
// CG framework used by those instances:
// https://www.ietf.org/archive/id/draft-irtf-cfrg-hybrid-kems-12.html#section-5.5

const kKemPqSeedLength = 64;
const kSeedLength = 32;
const kX25519KeyLength = 32;
const kP256KeyLength = 32;
const kP384KeyLength = 48;
const kEcPointPrefixLength = 1;
const kEcUncompressedPointPrefix = 0x04;
const kKemPq768EncapsulationKeyLength = 1184;
const kKemPq768CiphertextLength = 1088;
const kKemPq1024EncapsulationKeyLength = 1568;
const kKemPq1024CiphertextLength = 1568;
const kKemPq768Name = 'ML-KEM-768';
const kKemPq1024Name = 'ML-KEM-1024';
const kX25519Name = 'X25519';
const kEcdhName = 'ECDH';
const kEcKeyType = 'ec';
const kP256Name = 'P-256';
const kP384Name = 'P-384';
// P-256 and P-384 group order constants are used by RandomScalar.
// https://www.ietf.org/archive/id/draft-irtf-cfrg-concrete-hybrid-kems-04.html#section-3.1.1
// https://www.rfc-editor.org/rfc/rfc8017#section-4.2
const kP256Order = 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551n;
const kP384Order = 0xffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf581a0db248b0a77aecec196accc52973n;

const kEncapsulationUsages = ['encapsulateKey', 'encapsulateBits'];
const kDecapsulationUsages = ['decapsulateKey', 'decapsulateBits'];
const kUsages = createKeyUsages(kEncapsulationUsages, kDecapsulationUsages);

/**
 * Adds lengths that are the concatenation of the PQ KEM component and the
 * traditional group component.
 * @param {object} config
 * @returns {object}
 */
function withHybridLengths(config) {
  return {
    __proto__: null,
    ...config,
    encapsulationKeyLength:
      config.kemPqEncapsulationKeyLength + config.groupElementLength,
    ciphertextLength:
      config.kemPqCiphertextLength + config.groupElementLength,
  };
}

const kAlgorithms = {
  '__proto__': null,
  'MLKEM768-P256': withHybridLengths({
    name: 'MLKEM768-P256',
    kemPqName: kKemPq768Name,
    kemPqEncapsulationKeyLength: kKemPq768EncapsulationKeyLength,
    kemPqCiphertextLength: kKemPq768CiphertextLength,
    groupName: kEcdhName,
    groupKeyType: kEcKeyType,
    namedCurve: kP256Name,
    groupElementLength: kEcPointPrefixLength + 2 * kP256KeyLength,
    groupScalarLength: kP256KeyLength,
    groupSeedLength: 4 * kP256KeyLength,
    groupOrder: kP256Order,
    label: TypedArrayOf(
      Uint8Array,
      0x4d, 0x4c, 0x4b, 0x45, 0x4d, 0x37, 0x36,
      0x38, 0x2d, 0x50, 0x32, 0x35, 0x36, // "MLKEM768-P256"
    ),
  }),
  'MLKEM768-X25519': withHybridLengths({
    name: 'MLKEM768-X25519',
    kemPqName: kKemPq768Name,
    kemPqEncapsulationKeyLength: kKemPq768EncapsulationKeyLength,
    kemPqCiphertextLength: kKemPq768CiphertextLength,
    groupName: kX25519Name,
    groupKeyType: kX25519Name,
    // Curve25519 RandomScalar is identity and Exp is X25519.
    // https://www.rfc-editor.org/rfc/rfc7748#section-5
    groupElementLength: kX25519KeyLength,
    groupScalarLength: kX25519KeyLength,
    groupSeedLength: kX25519KeyLength,
    label: TypedArrayOf(
      Uint8Array,
      0x5c, 0x2e, 0x2f, 0x2f, 0x5e, 0x5c, // "\\.//^\\"
    ),
  }),
  'MLKEM1024-P384': withHybridLengths({
    name: 'MLKEM1024-P384',
    kemPqName: kKemPq1024Name,
    kemPqEncapsulationKeyLength: kKemPq1024EncapsulationKeyLength,
    kemPqCiphertextLength: kKemPq1024CiphertextLength,
    groupName: kEcdhName,
    groupKeyType: kEcKeyType,
    namedCurve: kP384Name,
    groupElementLength: kEcPointPrefixLength + 2 * kP384KeyLength,
    groupScalarLength: kP384KeyLength,
    groupSeedLength: kP384KeyLength,
    groupOrder: kP384Order,
    label: TypedArrayOf(
      Uint8Array,
      0x4d, 0x4c, 0x4b, 0x45, 0x4d, 0x31, 0x30,
      0x32, 0x34, 0x2d, 0x50, 0x33, 0x38, 0x34, // "MLKEM1024-P384"
    ),
  }),
};

/**
 * Wraps native or lower-level failures as WebCrypto OperationError failures.
 * @param {unknown} [cause]
 * @returns {DOMException}
 */
function operationFailure(cause) {
  return lazyDOMException(
    'The operation failed for an operation-specific reason',
    { name: 'OperationError', cause });
}

/**
 * Wraps native or lower-level failures as WebCrypto DataError failures.
 * @param {unknown} [cause]
 * @returns {DOMException}
 */
function dataFailure(cause) {
  return lazyDOMException('Invalid keyData', { name: 'DataError', cause });
}

/**
 * Copies supported BufferSource-like data into a new Uint8Array.
 * @param {ArrayBuffer | ArrayBufferView} data
 * @returns {Uint8Array}
 */
function copyBytes(data) {
  return new Uint8Array(getBufferSourceBytes(data));
}

/**
 * Concatenates byte arrays into a new Uint8Array of the expected length.
 * @param {Uint8Array[]} chunks
 * @param {number} length
 * @returns {Uint8Array}
 */
function concatBytes(chunks, length) {
  const output = new Uint8Array(length);
  let offset = 0;
  for (let n = 0; n < chunks.length; n++) {
    const chunk = chunks[n];
    TypedArrayPrototypeSet(output, chunk, offset);
    offset += TypedArrayPrototypeGetByteLength(chunk);
  }
  return output;
}

/**
 * Compares byte arrays by value.
 * @param {Uint8Array} a
 * @param {Uint8Array} b
 * @returns {boolean}
 */
function bytesEqual(a, b) {
  return TypedArrayPrototypeGetByteLength(a) === TypedArrayPrototypeGetByteLength(b) &&
    timingSafeEqual(a, b);
}

/**
 * Decodes a JWK base64url member.
 * @param {string} input
 * @returns {Uint8Array}
 */
function decodeJwkBase64Url(input) {
  return Buffer.from(input, 'base64url');
}

/**
 * Encodes JWK key material as unpadded base64url.
 * @param {Uint8Array} input
 * @returns {string}
 */
function base64urlEncode(input) {
  return Buffer.from(input).toString('base64url');
}

/**
 * Copies a Uint8Array's represented bytes into an ArrayBuffer result.
 * @param {Uint8Array} data
 * @returns {ArrayBuffer}
 */
function toArrayBuffer(data) {
  return TypedArrayPrototypeGetBuffer(copyBytes(data));
}

/**
 * Validates fixed-size key material.
 * @param {Uint8Array} data
 * @param {number} length
 */
function validateLength(data, length) {
  if (TypedArrayPrototypeGetByteLength(data) !== length) {
    throw lazyDOMException('Invalid keyData', 'DataError');
  }
}

/**
 * Splits bytes into the PQ KEM and traditional group portions.
 * @param {Uint8Array} data
 * @param {number} offset
 * @returns {{ head: Uint8Array, tail: Uint8Array }}
 */
function splitAt(data, offset) {
  return {
    __proto__: null,
    head: TypedArrayPrototypeSubarray(data, 0, offset),
    tail: TypedArrayPrototypeSubarray(data, offset),
  };
}

/**
 * Splits a Hybrid KEM raw-public key into its PQ and group public-key parts.
 * @param {Uint8Array} rawEncapsulationKey
 * @param {object} config
 * @returns {{ kemPq: Uint8Array, group: Uint8Array }}
 */
function splitEncapsulationKey(rawEncapsulationKey, config) {
  validateLength(rawEncapsulationKey, config.encapsulationKeyLength);
  const { head, tail } = splitAt(
    rawEncapsulationKey,
    config.kemPqEncapsulationKeyLength);
  return { __proto__: null, kemPq: head, group: tail };
}

/**
 * Copies and splits a Hybrid KEM ciphertext into its PQ and group parts.
 * @param {ArrayBuffer | ArrayBufferView} ciphertext
 * @param {object} config
 * @returns {{ kemPq: Uint8Array, group: Uint8Array }}
 */
function splitCiphertext(ciphertext, config) {
  const ciphertextBuffer = copyBytes(ciphertext);
  if (TypedArrayPrototypeGetByteLength(ciphertextBuffer) !==
      config.ciphertextLength) {
    throw operationFailure();
  }
  const { head, tail } = splitAt(
    ciphertextBuffer,
    config.kemPqCiphertextLength);
  return { __proto__: null, kemPq: head, group: copyBytes(tail) };
}

/**
 * Converts an octet string to a non-negative integer.
 * @param {Uint8Array} data
 * @returns {bigint}
 */
function os2ip(data) {
  let value = 0n;
  for (let n = 0; n < TypedArrayPrototypeGetLength(data); n++)
    value = (value << 8n) | BigInt(data[n]);
  return value;
}

/**
 * Derives a valid traditional group scalar from expanded seed bytes.
 * @param {Uint8Array} seed
 * @param {object} config
 * @returns {Uint8Array}
 */
function randomScalar(seed, config) {
  if (config.groupOrder === undefined)
    return copyBytes(seed);

  const { groupOrder, groupScalarLength } = config;
  for (let offset = 0;
    offset + groupScalarLength <= TypedArrayPrototypeGetLength(seed);
    offset += groupScalarLength) {
    const scalar = copyBytes(
      TypedArrayPrototypeSubarray(seed, offset, offset + groupScalarLength));
    const value = os2ip(scalar);
    if (value !== 0n && value < groupOrder)
      return scalar;
  }

  throw operationFailure();
}

/**
 * Imports raw key material into a native KeyObjectHandle.
 * @param {number} keyType
 * @param {Uint8Array} keyData
 * @param {number} format
 * @param {string} asymmetricKeyType
 * @param {string} [namedCurve]
 * @returns {KeyObjectHandle}
 */
function importRawKeyHandle(
  keyType,
  keyData,
  format,
  asymmetricKeyType,
  namedCurve) {
  const handle = new KeyObjectHandle();
  handle.init(
    keyType,
    keyData,
    format,
    asymmetricKeyType,
    null,
    namedCurve ?? null);
  if (asymmetricKeyType === kEcKeyType && !handle.checkEcKeyData())
    throw lazyDOMException('Invalid keyData', 'DataError');
  return handle;
}

/**
 * Imports the PQ KEM decapsulation seed.
 * @param {Uint8Array} seed
 * @param {object} config
 * @returns {KeyObjectHandle}
 */
function importKemPqDecapsulationHandle(seed, config) {
  return importRawKeyHandle(
    kKeyTypePrivate,
    seed,
    kKeyFormatRawSeed,
    config.kemPqName);
}

/**
 * Imports the PQ KEM encapsulation key.
 * @param {Uint8Array} rawEncapsulationKey
 * @param {object} config
 * @returns {KeyObjectHandle}
 */
function importKemPqEncapsulationHandle(rawEncapsulationKey, config) {
  return importRawKeyHandle(
    kKeyTypePublic,
    rawEncapsulationKey,
    kKeyFormatRawPublic,
    config.kemPqName);
}

/**
 * Imports the traditional group private scalar.
 * @param {Uint8Array} seed
 * @param {object} config
 * @returns {KeyObjectHandle}
 */
function importGroupPrivateHandle(seed, config) {
  return importRawKeyHandle(
    kKeyTypePrivate,
    seed,
    kKeyFormatRawPrivate,
    config.groupKeyType,
    config.namedCurve);
}

/**
 * Imports the traditional group public element.
 * @param {Uint8Array} groupElement
 * @param {object} config
 * @returns {KeyObjectHandle}
 */
function importGroupElementHandle(groupElement, config) {
  if (config.groupKeyType === kEcKeyType &&
      groupElement[0] !== kEcUncompressedPointPrefix) {
    throw lazyDOMException('Invalid keyData', 'DataError');
  }
  return importRawKeyHandle(
    kKeyTypePublic,
    groupElement,
    kKeyFormatRawPublic,
    config.groupKeyType,
    config.namedCurve);
}

/**
 * Exports the raw public key bytes from the PQ KEM handle.
 * @param {KeyObjectHandle} handle
 * @returns {Uint8Array}
 */
function rawKemPqEncapsulationKey(handle) {
  return copyBytes(handle.rawPublicKey());
}

/**
 * Exports the raw public element from the traditional group handle.
 * @param {KeyObjectHandle} handle
 * @param {object} config
 * @returns {Uint8Array}
 */
function rawGroupElement(handle, config) {
  if (config.groupKeyType === kEcKeyType) {
    return copyBytes(
      handle.exportECPublicRaw(POINT_CONVERSION_UNCOMPRESSED));
  }
  return copyBytes(handle.rawPublicKey());
}

/**
 * Builds the Hybrid KEM raw-public key from component public handles.
 * @param {KeyObjectHandle} kemPqHandle
 * @param {KeyObjectHandle} groupHandle
 * @param {object} config
 * @returns {Uint8Array}
 */
function rawEncapsulationKeyFromHandles(kemPqHandle, groupHandle, config) {
  return concatBytes([
    rawKemPqEncapsulationKey(kemPqHandle),
    rawGroupElement(groupHandle, config),
  ], config.encapsulationKeyLength);
}

/**
 * Imports both components of a Hybrid KEM public key.
 * @param {Uint8Array} kemPq
 * @param {Uint8Array} group
 * @param {object} config
 * @returns {{ kemPqHandle: KeyObjectHandle, groupHandle: KeyObjectHandle }}
 */
function importPublicKeyHandles(kemPq, group, config) {
  try {
    return {
      __proto__: null,
      kemPqHandle: importKemPqEncapsulationHandle(kemPq, config),
      groupHandle: importGroupElementHandle(group, config),
    };
  } catch (err) {
    throw dataFailure(err);
  }
}

/**
 * Imports both components of a Hybrid KEM private key.
 * @param {Uint8Array} seedPq
 * @param {Uint8Array} groupSeed
 * @param {object} config
 * @returns {{ kemPqHandle: KeyObjectHandle, groupHandle: KeyObjectHandle }}
 */
function importPrivateKeyHandles(seedPq, groupSeed, config) {
  try {
    return {
      __proto__: null,
      kemPqHandle: importKemPqDecapsulationHandle(seedPq, config),
      groupHandle: importGroupPrivateHandle(groupSeed, config),
    };
  } catch (err) {
    throw operationFailure(err);
  }
}

/**
 * Creates a Hybrid KEM public CryptoKey from imported component handles.
 * @param {KeyObjectHandle} kemPqHandle
 * @param {KeyObjectHandle} groupHandle
 * @param {object} config
 * @param {boolean} extractable
 * @param {Set<string> | string[]} keyUsages
 * @returns {InternalCryptoKey}
 */
function createPublicKeyFromComponents(
  kemPqHandle,
  groupHandle,
  config,
  extractable,
  keyUsages) {
  const usagesSet = new SafeSet(keyUsages);
  return new InternalCryptoKey(
    kemPqHandle,
    { name: config.name },
    getUsagesMask(usagesSet),
    extractable,
    groupHandle);
}

/**
 * Creates a Hybrid KEM private CryptoKey from imported component handles and seed.
 * @param {KeyObjectHandle} kemPqHandle
 * @param {KeyObjectHandle} groupHandle
 * @param {Uint8Array} seed
 * @param {object} config
 * @param {boolean} extractable
 * @param {Set<string> | string[]} keyUsages
 * @returns {InternalCryptoKey}
 */
function createPrivateKeyFromComponents(
  kemPqHandle,
  groupHandle,
  seed,
  config,
  extractable,
  keyUsages) {
  const usagesSet = new SafeSet(keyUsages);
  return new InternalCryptoKey(
    kemPqHandle,
    { name: config.name },
    getUsagesMask(usagesSet),
    extractable,
    groupHandle,
    seed);
}

/**
 * Creates a Hybrid KEM public CryptoKey from concatenated raw-public bytes.
 * @param {Uint8Array} rawEncapsulationKey
 * @param {object} config
 * @param {boolean} extractable
 * @param {Set<string> | string[]} keyUsages
 * @returns {InternalCryptoKey}
 */
function createPublicKeyFromRaw(
  rawEncapsulationKey,
  config,
  extractable,
  keyUsages) {
  const { kemPq, group } = splitEncapsulationKey(rawEncapsulationKey, config);
  const { kemPqHandle, groupHandle } =
    importPublicKeyHandles(kemPq, group, config);

  return createPublicKeyFromComponents(
    kemPqHandle,
    groupHandle,
    config,
    extractable,
    keyUsages);
}

/**
 * Expands the 32-byte Hybrid KEM seed into PQ KEM seed and group seed material.
 * @param {Uint8Array} seed
 * @param {object} config
 * @returns {{ seedPq: Uint8Array, groupSeed: Uint8Array }}
 */
function expandSeed(seed, config) {
  validateLength(seed, kSeedLength);

  // DeriveKeyPair(seed) expands seed into seed_PQ and seed_T.
  // https://www.ietf.org/archive/id/draft-irtf-cfrg-concrete-hybrid-kems-04.html#section-4.1
  const expanded = copyBytes(hash('shake256', seed, {
    outputEncoding: 'buffer',
    outputLength: kKemPqSeedLength + config.groupSeedLength,
  }));

  const { head, tail } = splitAt(expanded, kKemPqSeedLength);
  return {
    __proto__: null,
    seedPq: copyBytes(head),
    groupSeed: randomScalar(tail, config),
  };
}

/**
 * Derives the Hybrid KEM private key and, when requested, the matching public key.
 * @param {Uint8Array} seed
 * @param {object} config
 * @param {boolean} extractable
 * @param {Set<string> | string[]} decapsulationKeyUsages
 * @param {Set<string> | string[]} [encapsulationKeyUsages]
 * @returns {{ privateKey: InternalCryptoKey, publicKey?: InternalCryptoKey }}
 */
function deriveKeyPair(
  seed,
  config,
  extractable,
  decapsulationKeyUsages,
  encapsulationKeyUsages) {
  const { seedPq, groupSeed } = expandSeed(seed, config);
  const { kemPqHandle, groupHandle } =
    importPrivateKeyHandles(seedPq, groupSeed, config);

  const privateKey = createPrivateKeyFromComponents(
    kemPqHandle,
    groupHandle,
    seed,
    config,
    extractable,
    decapsulationKeyUsages);

  if (encapsulationKeyUsages === undefined)
    return { __proto__: null, privateKey };

  const publicKey = createPublicKeyFromRaw(
    rawEncapsulationKeyFromHandles(
      kemPqHandle,
      groupHandle,
      config),
    config,
    true,
    encapsulationKeyUsages);
  return { __proto__: null, privateKey, publicKey };
}

/**
 * Extracts both component handles stored in a Hybrid KEM CryptoKey.
 * @param {CryptoKey} key
 * @returns {{ kemPqHandle: KeyObjectHandle, groupHandle: KeyObjectHandle }}
 */
function getHybridKeyHandles(key) {
  const kemPqHandle = getCryptoKeyHandle(key);
  const groupHandle = getCryptoKeySecondaryHandle(key);
  if (groupHandle === undefined) {
    throw operationFailure();
  }
  return { __proto__: null, kemPqHandle, groupHandle };
}

/**
 * Extracts and copies the stored 32-byte Hybrid KEM seed from a private key.
 * @param {CryptoKey} key
 * @returns {Uint8Array}
 */
function getHybridSeed(key) {
  const seed = getCryptoKeySeedData(key);
  if (seed === undefined) {
    throw operationFailure();
  }
  return copyBytes(seed);
}

/**
 * Returns the algorithm config and component handles for a Hybrid KEM CryptoKey.
 * @param {CryptoKey} key
 * @returns {object}
 */
function getHybridKeyContext(key) {
  const config = kAlgorithms[getCryptoKeyAlgorithm(key).name];
  const { kemPqHandle, groupHandle } = getHybridKeyHandles(key);
  return { __proto__: null, config, kemPqHandle, groupHandle };
}

/**
 * Validates the CryptoKey type and returns its Hybrid KEM key context.
 * @param {CryptoKey} key
 * @param {'public' | 'private'} type
 * @param {string} message
 * @returns {object}
 */
function requireHybridKeyContext(key, type, message) {
  if (getCryptoKeyType(key) !== type)
    throw lazyDOMException(message, 'InvalidAccessError');
  return getHybridKeyContext(key);
}

/**
 * Runs ECDH/X25519 over the traditional group component.
 * @param {KeyObjectHandle} publicHandle
 * @param {KeyObjectHandle} privateHandle
 * @returns {Promise<Uint8Array>}
 */
function deriveGroupBits(publicHandle, privateHandle) {
  const bits = jobPromise(() => new DHBitsJob(
    kCryptoJobWebCrypto,
    publicHandle,
    undefined,
    undefined,
    undefined,
    undefined,
    privateHandle,
    undefined,
    undefined,
    undefined,
    undefined));
  return jobPromiseThen(
    bits,
    (bits) => copyBytes(bits),
    (err) => { throw operationFailure(err); });
}

/**
 * Applies the C2PRI combiner to the PQ and traditional shared secrets.
 * @param {Uint8Array} sharedSecretPq
 * @param {Uint8Array} sharedSecretT
 * @param {Uint8Array} ciphertextT
 * @param {Uint8Array} encapsulationKey
 * @param {object} config
 * @returns {Uint8Array}
 */
function combineSharedSecret(
  sharedSecretPq,
  sharedSecretT,
  ciphertextT,
  encapsulationKey,
  config) {
  // C2PRICombiner(ss_PQ, ss_T, ct_T, ek_T, Label).
  // https://www.ietf.org/archive/id/draft-irtf-cfrg-hybrid-kems-12.html#section-5.1.3
  const encapsulationKeyT = TypedArrayPrototypeSubarray(
    encapsulationKey,
    config.kemPqEncapsulationKeyLength);
  const inputLength =
    TypedArrayPrototypeGetByteLength(sharedSecretPq) +
    TypedArrayPrototypeGetByteLength(sharedSecretT) +
    TypedArrayPrototypeGetByteLength(ciphertextT) +
    TypedArrayPrototypeGetByteLength(encapsulationKeyT) +
    TypedArrayPrototypeGetByteLength(config.label);

  const input = concatBytes([
    sharedSecretPq,
    sharedSecretT,
    ciphertextT,
    encapsulationKeyT,
    config.label,
  ], inputLength);

  return copyBytes(hash('sha3-256', input, {
    outputEncoding: 'buffer',
  }));
}

/**
 * Splits requested Hybrid KEM key usages into private and public key usages.
 * @param {string} name
 * @param {string[]} keyUsages
 * @returns {{ privateUsages: Set<string>, publicUsages: Set<string> }}
 */
function getGenerateKeyUsages(name, keyUsages) {
  const usageSet = validateKeyUsages(keyUsages, kUsages.keygen, name);
  const usages = getKeyPairUsages(usageSet, kUsages);
  validateUsagesNotEmpty(usages.private);

  return {
    __proto__: null,
    privateUsages: usages.private,
    publicUsages: usages.public,
  };
}

/**
 * Imports a Hybrid KEM JWK, validating public/private consistency when `priv`
 * is present.
 * @param {object} input
 * @param {string} name
 * @param {object} config
 * @param {boolean} extractable
 * @param {Set<string>} usagesSet
 * @returns {InternalCryptoKey}
 */
function importJwkKey(input, name, config, extractable, usagesSet) {
  const isPublic = input.priv === undefined;
  verifyAcceptableKeyUse(
    name,
    usagesSet,
    isPublic ? kUsages.public : kUsages.private);
  validateJwk(input, 'AKP', extractable, usagesSet, 'enc');
  if (input.alg !== name) {
    throw lazyDOMException(
      'JWK "alg" Parameter and algorithm name mismatch',
      'DataError');
  }

  const rawEncapsulationKey = decodeJwkBase64Url(input.pub);
  validateLength(rawEncapsulationKey, config.encapsulationKeyLength);
  if (isPublic) {
    return createPublicKeyFromRaw(
      rawEncapsulationKey,
      config,
      extractable,
      usagesSet);
  }

  const { privateKey } = deriveKeyPair(
    decodeJwkBase64Url(input.priv),
    config,
    extractable,
    usagesSet);
  const { kemPqHandle, groupHandle } = getHybridKeyHandles(privateKey);
  const derivedRawEncapsulationKey =
    rawEncapsulationKeyFromHandles(kemPqHandle, groupHandle, config);
  if (!bytesEqual(derivedRawEncapsulationKey, rawEncapsulationKey))
    throw lazyDOMException('Invalid keyData', 'DataError');
  return privateKey;
}

/**
 * Exports a Hybrid KEM key as an AKP JWK.
 * @param {CryptoKey} key
 * @param {Uint8Array} rawEncapsulationKey
 * @param {object} config
 * @returns {object}
 */
function exportJwkKey(key, rawEncapsulationKey, config) {
  const jwk = {
    kty: 'AKP',
    alg: config.name,
    pub: base64urlEncode(rawEncapsulationKey),
    key_ops: ArrayPrototypeSlice(getCryptoKeyUsages(key), 0),
    ext: getCryptoKeyExtractable(key),
  };
  if (getCryptoKeyType(key) === 'private')
    setOwnProperty(jwk, 'priv', base64urlEncode(getHybridSeed(key)));
  return jwk;
}

/**
 * Implements generateKey for Hybrid KEM algorithms.
 * @param {object} algorithm
 * @param {boolean} extractable
 * @param {string[]} keyUsages
 * @returns {{ publicKey: InternalCryptoKey, privateKey: InternalCryptoKey }}
 */
function kemHybridGenerateKey(algorithm, extractable, keyUsages) {
  const { name } = algorithm;
  const config = kAlgorithms[name];
  const { privateUsages, publicUsages } =
    getGenerateKeyUsages(name, keyUsages);

  const keyPair = deriveKeyPair(
    copyBytes(randomBytes(kSeedLength)),
    config,
    extractable,
    privateUsages,
    publicUsages);
  return { publicKey: keyPair.publicKey, privateKey: keyPair.privateKey };
}

/**
 * Implements importKey for Hybrid KEM algorithms.
 * @param {'raw-public' | 'raw-seed' | 'jwk'} format
 * @param {ArrayBuffer | ArrayBufferView | object} input
 * @param {object} algorithm
 * @param {boolean} extractable
 * @param {string[]} keyUsages
 * @returns {InternalCryptoKey | undefined}
 */
function kemHybridImportKey(
  format,
  input,
  algorithm,
  extractable,
  keyUsages) {

  const { name } = algorithm;
  const config = kAlgorithms[name];
  const usagesSet = new SafeSet(keyUsages);

  switch (format) {
    case 'raw-public': {
      verifyAcceptableKeyUse(name, usagesSet, kUsages.public);
      return createPublicKeyFromRaw(
        copyBytes(input),
        config,
        extractable,
        usagesSet);
    }
    case 'raw-seed': {
      verifyAcceptableKeyUse(name, usagesSet, kUsages.private);
      return deriveKeyPair(
        copyBytes(input),
        config,
        extractable,
        usagesSet).privateKey;
    }
    case 'jwk':
      return importJwkKey(input, name, config, extractable, usagesSet);
    default:
      return undefined;
  }
}

/**
 * Implements exportKey for Hybrid KEM algorithms.
 * @param {CryptoKey} key
 * @param {'raw-public' | 'raw-seed' | 'jwk'} format
 * @returns {ArrayBuffer | object | undefined}
 */
function kemHybridExportKey(key, format) {
  const { config, kemPqHandle, groupHandle } = getHybridKeyContext(key);
  const rawEncapsulationKey =
    rawEncapsulationKeyFromHandles(kemPqHandle, groupHandle, config);
  switch (format) {
    case 'raw-public':
      return toArrayBuffer(rawEncapsulationKey);
    case 'raw-seed':
      if (getCryptoKeyType(key) === 'private') {
        return toArrayBuffer(getHybridSeed(key));
      }
      return undefined;
    case 'jwk':
      return exportJwkKey(key, rawEncapsulationKey, config);
    default:
      return undefined;
  }
}

/**
 * Implements getPublicKey for Hybrid KEM private keys.
 * @param {CryptoKey} privateKey
 * @param {string[]} keyUsages
 * @returns {InternalCryptoKey}
 */
function kemHybridGetPublicKey(privateKey, keyUsages) {
  const { config, kemPqHandle, groupHandle } =
    getHybridKeyContext(privateKey);
  const usageSet = new SafeSet(keyUsages);
  verifyAcceptableKeyUse(config.name, usageSet, kUsages.public);
  return createPublicKeyFromRaw(
    rawEncapsulationKeyFromHandles(kemPqHandle, groupHandle, config),
    config,
    true,
    usageSet);
}

/**
 * Generates the ephemeral traditional group private key for encapsulation.
 * @param {object} config
 * @returns {KeyObjectHandle}
 */
function generateEphemeralGroupScalar(config) {
  const seed = randomScalar(
    copyBytes(randomBytes(config.groupSeedLength)),
    config);
  return importGroupPrivateHandle(seed, config);
}

/**
 * Continues a WebCrypto job promise and routes synchronous continuation
 * failures to the provided reject callback.
 * @param {Promise} promise
 * @param {Function} reject
 * @param {Function} onFulfilled
 */
function continueJob(promise, reject, onFulfilled) {
  jobPromiseThen(promise, (value) => {
    try {
      onFulfilled(value);
    } catch (err) {
      reject(err);
    }
  }, reject);
}

/**
 * Continues a WebCrypto job promise and resolves with WebCrypto result
 * shielding for object results.
 * @param {Promise} promise
 * @param {Function} resolve
 * @param {Function} reject
 * @param {Function} onFulfilled
 */
function resolveJob(promise, resolve, reject, onFulfilled) {
  continueJob(promise, reject, (value) => {
    resolveWebCryptoResult(resolve, onFulfilled(value));
  });
}

/**
 * Prepares the traditional group side of encapsulation and starts its derive job.
 * @param {KeyObjectHandle} kemPqHandle
 * @param {KeyObjectHandle} groupHandle
 * @param {object} config
 * @returns {object}
 */
function prepareEncapsulationGroup(kemPqHandle, groupHandle, config) {
  const ephemeralGroupHandle = generateEphemeralGroupScalar(config);
  const ciphertextT = rawGroupElement(ephemeralGroupHandle, config);
  const sharedSecretT = deriveGroupBits(
    groupHandle,
    ephemeralGroupHandle);
  return {
    __proto__: null,
    ciphertextT,
    sharedSecretT,
    rawEncapsulationKey:
      rawEncapsulationKeyFromHandles(kemPqHandle, groupHandle, config),
  };
}

/**
 * Combines the PQ and traditional encapsulation outputs into WebCrypto's
 * encapsulation result object.
 * @param {object} encapsPq
 * @param {Uint8Array} sharedSecretT
 * @param {object} group
 * @param {object} config
 * @returns {{ sharedKey: ArrayBuffer, ciphertext: ArrayBuffer }}
 */
function hybridEncapsulationResult(encapsPq, sharedSecretT, group, config) {
  const sharedKey = combineSharedSecret(
    copyBytes(encapsPq.sharedKey),
    sharedSecretT,
    group.ciphertextT,
    group.rawEncapsulationKey,
    config);
  const ciphertext = concatBytes([
    copyBytes(encapsPq.ciphertext),
    group.ciphertextT,
  ], config.ciphertextLength);

  return {
    sharedKey: toArrayBuffer(sharedKey),
    ciphertext: toArrayBuffer(ciphertext),
  };
}

/**
 * Imports the traditional group ciphertext component as a public key handle.
 * @param {Uint8Array} ciphertextT
 * @param {object} config
 * @returns {KeyObjectHandle}
 */
function importCiphertextGroupHandle(ciphertextT, config) {
  try {
    return importGroupElementHandle(ciphertextT, config);
  } catch (err) {
    throw operationFailure(err);
  }
}

/**
 * Implements encapsulateBits for Hybrid KEM algorithms.
 * @param {CryptoKey} encapsulationKey
 * @returns {Promise<{ sharedKey: ArrayBuffer, ciphertext: ArrayBuffer }>}
 */
function kemHybridEncaps(encapsulationKey) {
  const { config, kemPqHandle, groupHandle } = requireHybridKeyContext(
    encapsulationKey,
    'public',
    'Key must be a public key');
  const encapsPq = jobPromise(() => new KEMEncapsulateJob(
    kCryptoJobWebCrypto,
    kemPqHandle,
    undefined,
    undefined,
    undefined,
    undefined));
  const {
    promise,
    resolve,
    reject,
  } = PromiseWithResolvers();

  continueJob(encapsPq, reject, (encapsPq) => {
    const group = prepareEncapsulationGroup(
      kemPqHandle,
      groupHandle,
      config);
    resolveJob(group.sharedSecretT, resolve, reject, (sharedSecretT) =>
      hybridEncapsulationResult(encapsPq, sharedSecretT, group, config));
  });

  return promise;
}

/**
 * Implements decapsulateBits for Hybrid KEM algorithms.
 * @param {CryptoKey} decapsulationKey
 * @param {ArrayBuffer | ArrayBufferView} ciphertext
 * @returns {Promise<ArrayBuffer>}
 */
function kemHybridDecaps(decapsulationKey, ciphertext) {
  const { config, kemPqHandle, groupHandle } = requireHybridKeyContext(
    decapsulationKey,
    'private',
    'Key must be a private key');
  const { kemPq: ciphertextPq, group: ciphertextT } =
    splitCiphertext(ciphertext, config);
  const groupPublicHandle =
    importCiphertextGroupHandle(ciphertextT, config);

  const sharedSecretPq = jobPromise(() => new KEMDecapsulateJob(
    kCryptoJobWebCrypto,
    kemPqHandle,
    undefined,
    undefined,
    undefined,
    undefined,
    ciphertextPq));
  const {
    promise,
    resolve,
    reject,
  } = PromiseWithResolvers();

  continueJob(sharedSecretPq, reject, (sharedSecretPq) => {
    const sharedSecretT = deriveGroupBits(
      groupPublicHandle,
      groupHandle);
    resolveJob(sharedSecretT, resolve, reject, (sharedSecretT) =>
      toArrayBuffer(combineSharedSecret(
        copyBytes(sharedSecretPq),
        sharedSecretT,
        ciphertextT,
        rawEncapsulationKeyFromHandles(kemPqHandle, groupHandle, config),
        config)));
  });

  return promise;
}

module.exports = {
  kemHybridDecaps,
  kemHybridEncaps,
  kemHybridExportKey,
  kemHybridGenerateKey,
  kemHybridGetPublicKey,
  kemHybridImportKey,
};
