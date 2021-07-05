'use strict';

const {
  ArrayPrototypeIncludes,
  ArrayPrototypePush,
  BigInt,
  FunctionPrototypeBind,
  Number,
  ObjectKeys,
  Promise,
  StringPrototypeToLowerCase,
  Symbol,
} = primordials;

const {
  getCiphers: _getCiphers,
  getCurves: _getCurves,
  getHashes: _getHashes,
  setEngine: _setEngine,
  secureHeapUsed: _secureHeapUsed,
} = internalBinding('crypto');

const { getOptionValue } = require('internal/options');

const {
  crypto: {
    ENGINE_METHOD_ALL
  }
} = internalBinding('constants');

const normalizeHashName = require('internal/crypto/hashnames');

const {
  hideStackFrames,
  codes: {
    ERR_CRYPTO_ENGINE_UNKNOWN,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_OUT_OF_RANGE,
  }
} = require('internal/errors');

const {
  validateArray,
  validateNumber,
  validateString
} = require('internal/validators');

const { Buffer } = require('buffer');

const {
  cachedResult,
  filterDuplicateStrings,
  lazyDOMException,
} = require('internal/util');

const {
  isArrayBufferView,
  isAnyArrayBuffer,
} = require('internal/util/types');

const kHandle = Symbol('kHandle');
const kKeyObject = Symbol('kKeyObject');

const lazyRequireCache = {};

function lazyRequire(name) {
  let ret = lazyRequireCache[name];
  if (ret === undefined)
    ret = lazyRequireCache[name] = require(name);
  return ret;
}

var defaultEncoding = 'buffer';

function setDefaultEncoding(val) {
  defaultEncoding = val;
}

function getDefaultEncoding() {
  return defaultEncoding;
}

// This is here because many functions accepted binary strings without
// any explicit encoding in older versions of node, and we don't want
// to break them unnecessarily.
function toBuf(val, encoding) {
  if (typeof val === 'string') {
    if (encoding === 'buffer')
      encoding = 'utf8';
    return Buffer.from(val, encoding);
  }
  return val;
}

const getCiphers = cachedResult(() => filterDuplicateStrings(_getCiphers()));
const getHashes = cachedResult(() => filterDuplicateStrings(_getHashes()));
const getCurves = cachedResult(() => filterDuplicateStrings(_getCurves()));

function setEngine(id, flags) {
  validateString(id, 'id');
  if (flags)
    validateNumber(flags, 'flags');
  flags = flags >>> 0;

  // Use provided engine for everything by default
  if (flags === 0)
    flags = ENGINE_METHOD_ALL;

  if (!_setEngine(id, flags))
    throw new ERR_CRYPTO_ENGINE_UNKNOWN(id);
}

const getArrayBufferOrView = hideStackFrames((buffer, name, encoding) => {
  if (isAnyArrayBuffer(buffer))
    return buffer;
  if (typeof buffer === 'string') {
    if (encoding === 'buffer')
      encoding = 'utf8';
    return Buffer.from(buffer, encoding);
  }
  if (!isArrayBufferView(buffer)) {
    throw new ERR_INVALID_ARG_TYPE(
      name,
      [
        'string',
        'ArrayBuffer',
        'Buffer',
        'TypedArray',
        'DataView',
      ],
      buffer
    );
  }
  return buffer;
});

// The maximum buffer size that we'll support in the WebCrypto impl
const kMaxBufferLength = (2 ** 31) - 1;

// The EC named curves that we currently support via the Web Crypto API.
const kNamedCurveAliases = {
  'P-256': 'prime256v1',
  'P-384': 'secp384r1',
  'P-521': 'secp521r1',
  'NODE-ED25519': 'ed25519',
  'NODE-ED448': 'ed448',
  'NODE-X25519': 'x25519',
  'NODE-X448': 'x448',
};

const kAesKeyLengths = [128, 192, 256];

// These are the only algorithms we currently support
// via the Web Crypto API
const kAlgorithms = {
  'rsassa-pkcs1-v1_5': 'RSASSA-PKCS1-v1_5',
  'rsa-pss': 'RSA-PSS',
  'rsa-oaep': 'RSA-OAEP',
  'ecdsa': 'ECDSA',
  'ecdh': 'ECDH',
  'aes-ctr': 'AES-CTR',
  'aes-cbc': 'AES-CBC',
  'aes-gcm': 'AES-GCM',
  'aes-kw': 'AES-KW',
  'hmac': 'HMAC',
  'sha-1': 'SHA-1',
  'sha-256': 'SHA-256',
  'sha-384': 'SHA-384',
  'sha-512': 'SHA-512',
  'hkdf': 'HKDF',
  'pbkdf2': 'PBKDF2',
  // Following here are Node.js specific extensions. All
  // should be prefixed with 'node-'
  'node-dsa': 'NODE-DSA',
  'node-dh': 'NODE-DH',
  'node-scrypt': 'NODE-SCRYPT',
  'node-ed25519': 'NODE-ED25519',
  'node-ed448': 'NODE-ED448',
};
const kAlgorithmsKeys = ObjectKeys(kAlgorithms);

// These are the only export and import formats we currently
// support via the Web Crypto API
const kExportFormats = [
  'raw',
  'pkcs8',
  'spki',
  'jwk',
  'node.keyObject'];

// These are the only hash algorithms we currently support via
// the Web Crypto API.
const kHashTypes = [
  'SHA-1',
  'SHA-256',
  'SHA-384',
  'SHA-512',
];

function validateMaxBufferLength(data, name) {
  if (data.byteLength > kMaxBufferLength) {
    throw lazyDOMException(
      `${name} must be less than ${kMaxBufferLength + 1} bits`,
      'OperationError');
  }
}

function normalizeAlgorithm(algorithm, label = 'algorithm') {
  if (algorithm != null) {
    if (typeof algorithm === 'string')
      algorithm = { name: algorithm };

    if (typeof algorithm === 'object') {
      const { name } = algorithm;
      let hash;
      if (typeof name !== 'string' ||
          !ArrayPrototypeIncludes(
            kAlgorithmsKeys,
            StringPrototypeToLowerCase(name))) {
        throw lazyDOMException('Unrecognized name.', 'NotSupportedError');
      }
      if (algorithm.hash !== undefined) {
        hash = normalizeAlgorithm(algorithm.hash, 'algorithm.hash');
        if (!ArrayPrototypeIncludes(kHashTypes, hash.name))
          throw lazyDOMException('Unrecognized name.', 'NotSupportedError');
      }
      return {
        ...algorithm,
        name: kAlgorithms[StringPrototypeToLowerCase(name)],
        hash,
      };
    }
  }
  throw lazyDOMException('Unrecognized name.', 'NotSupportedError');
}

function hasAnyNotIn(set, checks) {
  for (const s of set)
    if (!ArrayPrototypeIncludes(checks, s))
      return true;
  return false;
}

function validateBitLength(length, name, required = false) {
  if (length !== undefined || required) {
    validateNumber(length, name);
    if (length < 0)
      throw new ERR_OUT_OF_RANGE(name, '> 0');
    if (length % 8) {
      throw new ERR_INVALID_ARG_VALUE(
        name,
        length,
        'must be a multiple of 8');
    }
  }
}

function validateByteLength(buf, name, target) {
  if (buf.byteLength !== target) {
    throw lazyDOMException(
      `${name} must contain exactly ${target} bytes`,
      'OperationError');
  }
}

const validateByteSource = hideStackFrames((val, name) => {
  val = toBuf(val);

  if (isAnyArrayBuffer(val) || isArrayBufferView(val))
    return;

  throw new ERR_INVALID_ARG_TYPE(
    name,
    [
      'string',
      'ArrayBuffer',
      'TypedArray',
      'DataView',
      'Buffer',
    ],
    val);
});

function onDone(resolve, reject, err, result) {
  if (err) return reject(err);
  resolve(result);
}

function jobPromise(job) {
  return new Promise((resolve, reject) => {
    job.ondone = FunctionPrototypeBind(onDone, job, resolve, reject);
    job.run();
  });
}

// In WebCrypto, the publicExponent option in RSA is represented as a
// WebIDL "BigInteger"... that is, a Uint8Array that allows an arbitrary
// number of leading zero bits. Our conventional APIs for reading
// an unsigned int from a Buffer are not adequate. The implementation
// here is adapted from the chromium implementation here:
// https://github.com/chromium/chromium/blob/HEAD/third_party/blink/public/platform/web_crypto_algorithm_params.h, but ported to JavaScript
// Returns undefined if the conversion was unsuccessful.
function bigIntArrayToUnsignedInt(input) {
  let result = 0;

  for (let n = 0; n < input.length; ++n) {
    const n_reversed = input.length - n - 1;
    if (n_reversed >= 4 && input[n])
      return;  // Too large
    result |= input[n] << 8 * n_reversed;
  }

  return result;
}

function bigIntArrayToUnsignedBigInt(input) {
  let result = 0n;

  for (let n = 0; n < input.length; ++n) {
    const n_reversed = input.length - n - 1;
    result |= BigInt(input[n]) << 8n * BigInt(n_reversed);
  }

  return result;
}

function getStringOption(options, key) {
  let value;
  if (options && (value = options[key]) != null)
    validateString(value, `options.${key}`);
  return value;
}

function getUsagesUnion(usageSet, ...usages) {
  const newset = [];
  for (let n = 0; n < usages.length; n++) {
    if (usageSet.has(usages[n]))
      ArrayPrototypePush(newset, usages[n]);
  }
  return newset;
}

function getHashLength(name) {
  switch (name) {
    case 'SHA-1': return 160;
    case 'SHA-256': return 256;
    case 'SHA-384': return 384;
    case 'SHA-512': return 512;
  }
}

const kKeyOps = {
  sign: 1,
  verify: 2,
  encrypt: 3,
  decrypt: 4,
  wrapKey: 5,
  unwrapKey: 6,
  deriveKey: 7,
  deriveBits: 8,
};

function validateKeyOps(keyOps, usagesSet) {
  if (keyOps === undefined) return;
  validateArray(keyOps, 'keyData.key_ops');
  let flags = 0;
  for (let n = 0; n < keyOps.length; n++) {
    const op = keyOps[n];
    const op_flag = kKeyOps[op];
    // Skipping unknown key ops
    if (op_flag === undefined)
      continue;
    // Have we seen it already? if so, error
    if (flags & (1 << op_flag))
      throw lazyDOMException('Duplicate key operation', 'DataError');
    flags |= (1 << op_flag);

    // TODO(@jasnell): RFC7517 section 4.3 strong recommends validating
    // key usage combinations. Specifically, it says that unrelated key
    // ops SHOULD NOT be used together. We're not yet validating that here.
  }

  if (usagesSet !== undefined) {
    for (const use of usagesSet) {
      if (!ArrayPrototypeIncludes(keyOps, use)) {
        throw lazyDOMException(
          'Key operations and usage mismatch',
          'DataError');
      }
    }
  }
}

function secureHeapUsed() {
  const val = _secureHeapUsed();
  if (val === undefined)
    return { total: 0, used: 0, utilization: 0, min: 0 };
  const used = Number(_secureHeapUsed());
  const total = Number(getOptionValue('--secure-heap'));
  const min = Number(getOptionValue('--secure-heap-min'));
  const utilization = used / total;
  return { total, used, utilization, min };
}

module.exports = {
  getArrayBufferOrView,
  getCiphers,
  getCurves,
  getDefaultEncoding,
  getHashes,
  kHandle,
  kKeyObject,
  setDefaultEncoding,
  setEngine,
  toBuf,

  kHashTypes,
  kNamedCurveAliases,
  kAesKeyLengths,
  kExportFormats,
  normalizeAlgorithm,
  normalizeHashName,
  hasAnyNotIn,
  validateBitLength,
  validateByteLength,
  validateByteSource,
  validateKeyOps,
  jobPromise,
  lazyRequire,
  validateMaxBufferLength,
  bigIntArrayToUnsignedBigInt,
  bigIntArrayToUnsignedInt,
  getStringOption,
  getUsagesUnion,
  getHashLength,
  secureHeapUsed,
};
