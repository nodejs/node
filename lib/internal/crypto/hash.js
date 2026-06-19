'use strict';

const {
  FunctionPrototypeCall,
  ObjectSetPrototypeOf,
  StringPrototypeToLowerCase,
  Symbol,
  TypedArrayPrototypeGetBuffer,
} = primordials;

const {
  CShakeJob,
  Hash: _Hash,
  HashJob,
  Hmac: _Hmac,
  kCryptoJobWebCrypto,
  oneShotDigest,
  TurboShakeJob,
  KangarooTwelveJob,
} = internalBinding('crypto');

const {
  getStringOption,
  jobPromise,
  jobPromiseThen,
  normalizeHashName,
  numBitsToBytes,
  truncateToBitLength,
  validateMaxBufferLength,
  kHandle,
  getCachedHashId,
  getHashCache,
} = require('internal/crypto/util');

const {
  prepareSecretKey,
} = require('internal/crypto/keys');

const {
  lazyDOMException,
  normalizeEncoding,
  encodingsMap,
  getDeprecationWarningEmitter,
} = require('internal/util');

const {
  Buffer,
} = require('buffer');

const {
  codes: {
    ERR_CRYPTO_HASH_FINALIZED,
    ERR_CRYPTO_HASH_UPDATE_FAILED,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');

const {
  validateEncoding,
  validateString,
  validateObject,
  validateUint32,
} = require('internal/validators');

const {
  isArrayBufferView,
} = require('internal/util/types');

const LazyTransform = require('internal/streams/lazy_transform');

const kState = Symbol('kState');
const kFinalized = Symbol('kFinalized');

const emitHmacDigestDeprecation = getDeprecationWarningEmitter(
  'DEP0206',
  'Calling Hmac.digest() more than once is deprecated.',
);

function Hash(algorithm, options) {
  if (!new.target)
    return new Hash(algorithm, options);
  const isCopy = algorithm instanceof _Hash;
  if (!isCopy)
    validateString(algorithm, 'algorithm');
  let xofLen = typeof options === 'object' && options !== null ?
    options.outputLength : undefined;
  if (xofLen !== undefined) {
    validateUint32(xofLen, 'options.outputLength');
    // Coerce -0 to +0.
    xofLen += 0;
  }
  // Lookup the cached ID from JS land because it's faster than decoding
  // the string in C++ land.
  const algorithmId = isCopy ? -1 : getCachedHashId(algorithm);
  this[kHandle] = new _Hash(algorithm, xofLen, algorithmId, getHashCache());
  this[kState] = {
    [kFinalized]: false,
  };
  FunctionPrototypeCall(LazyTransform, this, options);
}

ObjectSetPrototypeOf(Hash.prototype, LazyTransform.prototype);
ObjectSetPrototypeOf(Hash, LazyTransform);

Hash.prototype.copy = function copy(options) {
  const state = this[kState];
  if (state[kFinalized])
    throw new ERR_CRYPTO_HASH_FINALIZED();

  return new Hash(this[kHandle], options);
};

Hash.prototype._transform = function _transform(chunk, encoding, callback) {
  if (!this[kHandle].update(chunk, encoding))
    return callback(new ERR_CRYPTO_HASH_UPDATE_FAILED());
  callback();
};

Hash.prototype._flush = function _flush(callback) {
  this.push(this[kHandle].digest());
  callback();
};

Hash.prototype.update = function update(data, encoding) {
  const state = this[kState];
  if (state[kFinalized])
    throw new ERR_CRYPTO_HASH_FINALIZED();

  if (typeof data === 'string') {
    validateEncoding(data, encoding);
  } else if (!isArrayBufferView(data)) {
    throw new ERR_INVALID_ARG_TYPE(
      'data', ['string', 'Buffer', 'TypedArray', 'DataView'], data);
  }

  if (!this[kHandle].update(data, encoding))
    throw new ERR_CRYPTO_HASH_UPDATE_FAILED();
  return this;
};


Hash.prototype.digest = function digest(outputEncoding) {
  const state = this[kState];
  if (state[kFinalized])
    throw new ERR_CRYPTO_HASH_FINALIZED();

  // Explicit conversion of truthy values for backward compatibility.
  const ret = this[kHandle].digest(outputEncoding && `${outputEncoding}`);
  state[kFinalized] = true;
  return ret;
};

function Hmac(hmac, key, options) {
  if (!(this instanceof Hmac))
    return new Hmac(hmac, key, options);
  validateString(hmac, 'hmac');
  const encoding = getStringOption(options, 'encoding');
  key = prepareSecretKey(key, encoding);
  this[kHandle] = new _Hmac();
  this[kHandle].init(hmac, key);
  this[kState] = {
    [kFinalized]: false,
  };
  FunctionPrototypeCall(LazyTransform, this, options);
}

ObjectSetPrototypeOf(Hmac.prototype, LazyTransform.prototype);
ObjectSetPrototypeOf(Hmac, LazyTransform);

Hmac.prototype.update = Hash.prototype.update;

Hmac.prototype.digest = function digest(outputEncoding) {
  const state = this[kState];

  if (state[kFinalized]) {
    emitHmacDigestDeprecation();
    const buf = Buffer.from('');
    if (outputEncoding && outputEncoding !== 'buffer')
      return buf.toString(outputEncoding);
    return buf;
  }

  // Explicit conversion of truthy values for backward compatibility.
  const ret = this[kHandle].digest(outputEncoding && `${outputEncoding}`);
  state[kFinalized] = true;
  return ret;
};

Hmac.prototype._flush = Hash.prototype._flush;
Hmac.prototype._transform = Hash.prototype._transform;

// Implementation for WebCrypto subtle.digest()

function asyncDigest(algorithm, data) {
  validateMaxBufferLength(data, 'data');

  switch (algorithm.name) {
    case 'SHA-1':
      // Fall through
    case 'SHA-256':
      // Fall through
    case 'SHA-384':
      // Fall through
    case 'SHA-512':
      // Fall through
    case 'SHA3-256':
      // Fall through
    case 'SHA3-384':
      // Fall through
    case 'SHA3-512':
      return jobPromise(() => new HashJob(
        kCryptoJobWebCrypto,
        normalizeHashName(algorithm.name),
        data));
    case 'cSHAKE128':
      // Fall through
    case 'cSHAKE256': {
      const outputLength = algorithm.outputLength;
      if (algorithm.functionName?.byteLength ||
          algorithm.customization?.byteLength) {
        if (CShakeJob === undefined) {
          throw lazyDOMException(
            'Non-empty CShakeParams functionName or customization is not supported',
            'NotSupportedError');
        }

        return jobPromise(() => new CShakeJob(
          kCryptoJobWebCrypto,
          algorithm.name,
          data,
          algorithm.functionName,
          algorithm.customization,
          outputLength));
      }

      const bits = jobPromise(() => new HashJob(
        kCryptoJobWebCrypto,
        normalizeHashName(algorithm.name),
        data,
        numBitsToBytes(outputLength) * 8));
      if (outputLength % 8 === 0)
        return bits;
      return jobPromiseThen(bits, (bits) =>
        TypedArrayPrototypeGetBuffer(truncateToBitLength(outputLength, bits)));
    }
    case 'TurboSHAKE128':
      // Fall through
    case 'TurboSHAKE256':
      return jobPromise(() => new TurboShakeJob(
        kCryptoJobWebCrypto,
        algorithm.name,
        algorithm.domainSeparation ?? 0x1f,
        algorithm.outputLength / 8,
        data));
    case 'KT128':
      // Fall through
    case 'KT256':
      return jobPromise(() => new KangarooTwelveJob(
        kCryptoJobWebCrypto,
        algorithm.name,
        algorithm.customization,
        algorithm.outputLength / 8,
        data));
    /* c8 ignore start */
    default: {
      const assert = require('internal/assert');
      assert.fail('Unreachable code');
    }
    /* c8 ignore stop */
  }
}

function hash(algorithm, input, options) {
  validateString(algorithm, 'algorithm');
  if (typeof input !== 'string' && !isArrayBufferView(input)) {
    throw new ERR_INVALID_ARG_TYPE('input', ['Buffer', 'TypedArray', 'DataView', 'string'], input);
  }
  let outputEncoding;
  let outputLength;

  if (typeof options === 'string') {
    outputEncoding = options;
  } else if (options !== undefined) {
    validateObject(options, 'options');
    outputLength = options.outputLength;
    outputEncoding = options.outputEncoding;
  }

  outputEncoding ??= 'hex';

  let normalized = outputEncoding;
  // Fast case: if it's 'hex', we don't need to validate it further.
  if (normalized !== 'hex') {
    validateString(outputEncoding, 'outputEncoding');
    normalized = normalizeEncoding(outputEncoding);
    // If the encoding is invalid, normalizeEncoding() returns undefined.
    if (normalized === undefined) {
      // normalizeEncoding() doesn't handle 'buffer'.
      if (StringPrototypeToLowerCase(outputEncoding) === 'buffer') {
        normalized = 'buffer';
      } else {
        throw new ERR_INVALID_ARG_VALUE('outputEncoding', outputEncoding);
      }
    }
  }

  if (outputLength !== undefined) {
    validateUint32(outputLength, 'outputLength');
    // Coerce -0 to +0.
    outputLength += 0;
  }

  return oneShotDigest(algorithm, getCachedHashId(algorithm), getHashCache(),
                       input, normalized, encodingsMap[normalized], outputLength);
}

module.exports = {
  Hash,
  Hmac,
  asyncDigest,
  hash,
};
