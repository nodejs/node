'use strict';

const {
  ArrayBuffer,
  FunctionPrototypeCall,
  PromiseResolve,
  TypedArrayPrototypeGetByteLength,
  Uint8Array,
} = primordials;

const {
  HKDFJob,
  kCryptoJobAsync,
  kCryptoJobSync,
  kCryptoJobWebCrypto,
} = internalBinding('crypto');

const {
  validateFunction,
  validateInteger,
  validateString,
} = require('internal/validators');

const { kMaxLength } = require('buffer');

const {
  getBufferSourceByteLength,
  jobPromise,
  normalizeHashName,
  toBuf,
  validateByteSource,
} = require('internal/crypto/util');

const {
  getCryptoKeyHandle,
  isKeyObject,
  prepareSecretKey,
} = require('internal/crypto/keys');

const {
  lazyDOMException,
} = require('internal/util');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
  isSharedArrayBuffer,
} = require('internal/util/types');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_OUT_OF_RANGE,
  },
  hideStackFrames,
} = require('internal/errors');

function getByteSourceByteLength(source) {
  if (isSharedArrayBuffer(source)) {
    return TypedArrayPrototypeGetByteLength(new Uint8Array(source));
  }
  return getBufferSourceByteLength(source);
}

const validateParameters = hideStackFrames((hash, key, salt, info, length) => {
  validateString.withoutStackTrace(hash, 'digest');
  key = prepareKey(key);
  salt = validateByteSource.withoutStackTrace(salt, 'salt');
  info = validateByteSource.withoutStackTrace(info, 'info');

  validateInteger.withoutStackTrace(length, 'length', 0, kMaxLength);
  // Coerce -0 to +0.
  length += 0;

  const infoByteLength = getByteSourceByteLength(info);
  if (infoByteLength > 1024) {
    throw new ERR_OUT_OF_RANGE.HideStackFramesError(
      'info',
      'must not contain more than 1024 bytes',
      infoByteLength);
  }

  return {
    hash,
    key,
    salt,
    info,
    length,
  };
});

function prepareKey(key) {
  if (isKeyObject(key))
    return prepareSecretKey(key);

  if (isAnyArrayBuffer(key))
    return key;

  key = toBuf(key);

  if (!isArrayBufferView(key)) {
    throw new ERR_INVALID_ARG_TYPE(
      'ikm',
      [
        'string',
        'SecretKeyObject',
        'ArrayBuffer',
        'TypedArray',
        'DataView',
        'Buffer',
      ],
      key);
  }

  return key;
}

function createHkdfJob(mode, params) {
  return new HKDFJob(
    mode,
    params.hash,
    params.key,
    params.salt,
    params.info,
    params.length);
}

function hkdf(hash, key, salt, info, length, callback) {
  const params = validateParameters(hash, key, salt, info, length);
  validateFunction(callback, 'callback');
  const job = createHkdfJob(kCryptoJobAsync, params);

  job.ondone = (error, bits) => {
    if (error) return FunctionPrototypeCall(callback, job, error);
    FunctionPrototypeCall(callback, job, null, bits);
  };

  job.run();
}

function hkdfSync(hash, key, salt, info, length) {
  const params = validateParameters(hash, key, salt, info, length);
  const job = createHkdfJob(kCryptoJobSync, params);
  const { 0: err, 1: bits } = job.run();
  if (err !== undefined)
    throw err;

  return bits;
}

function getDigestSizeInBytes(name) {
  switch (name) {
    case 'SHA-1':
      return 20;
    case 'SHA-256': // Fall through
    case 'SHA3-256':
      return 32;
    case 'SHA-384': // Fall through
    case 'SHA3-384':
      return 48;
    case 'SHA-512': // Fall through
    case 'SHA3-512':
      return 64;
  }
}

function validateHkdfDeriveBitsLength(length, hash) {
  if (length === null)
    throw lazyDOMException('length cannot be null', 'OperationError');
  if (length % 8) {
    throw lazyDOMException(
      'length must be a multiple of 8',
      'OperationError');
  }
  if (length > 255 * getDigestSizeInBytes(hash.name) * 8) {
    throw lazyDOMException(
      'length exceeds the maximum derived bit length',
      'OperationError');
  }
}

function hkdfDeriveBits(algorithm, baseKey, length) {
  const { hash, salt, info } = algorithm;
  validateHkdfDeriveBitsLength(length, hash);

  if (length === 0)
    return PromiseResolve(new ArrayBuffer(0));

  return jobPromise(() => new HKDFJob(
    kCryptoJobWebCrypto,
    normalizeHashName(hash.name),
    getCryptoKeyHandle(baseKey),
    salt,
    info,
    length / 8));
}

module.exports = {
  hkdf,
  hkdfSync,
  hkdfDeriveBits,
  validateHkdfDeriveBitsLength,
};
