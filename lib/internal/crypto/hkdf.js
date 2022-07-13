'use strict';

const {
  FunctionPrototypeCall,
  Promise,
  Uint8Array,
} = primordials;

const {
  HKDFJob,
  kCryptoJobAsync,
  kCryptoJobSync,
} = internalBinding('crypto');

const {
  validateFunction,
  validateInteger,
  validateString,
  validateUint32,
} = require('internal/validators');

const { kMaxLength } = require('buffer');

const {
  getArrayBufferOrView,
  normalizeHashName,
  toBuf,
  validateByteSource,
  kKeyObject,
} = require('internal/crypto/util');

const {
  createSecretKey,
  isKeyObject,
} = require('internal/crypto/keys');

const {
  lazyDOMException,
} = require('internal/util');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_OUT_OF_RANGE,
    ERR_MISSING_OPTION,
  },
  hideStackFrames,
} = require('internal/errors');

const validateParameters = hideStackFrames((hash, key, salt, info, length) => {
  validateString(hash, 'digest');
  key = prepareKey(key);
  salt = validateByteSource(salt, 'salt');
  info = validateByteSource(info, 'info');

  validateInteger(length, 'length', 0, kMaxLength);

  if (info.byteLength > 1024) {
    throw ERR_OUT_OF_RANGE(
      'info',
      'must not contain more than 1024 bytes',
      info.byteLength);
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
    return key;

  // TODO(@jasnell): createSecretKey should allow using an ArrayBuffer
  if (isAnyArrayBuffer(key))
    return createSecretKey(new Uint8Array(key));

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

  return createSecretKey(key);
}

function hkdf(hash, key, salt, info, length, callback) {
  ({
    hash,
    key,
    salt,
    info,
    length,
  } = validateParameters(hash, key, salt, info, length));

  validateFunction(callback, 'callback');

  const job = new HKDFJob(kCryptoJobAsync, hash, key, salt, info, length);

  job.ondone = (error, bits) => {
    if (error) return FunctionPrototypeCall(callback, job, error);
    FunctionPrototypeCall(callback, job, null, bits);
  };

  job.run();
}

function hkdfSync(hash, key, salt, info, length) {
  ({
    hash,
    key,
    salt,
    info,
    length,
  } = validateParameters(hash, key, salt, info, length));

  const job = new HKDFJob(kCryptoJobSync, hash, key, salt, info, length);
  const { 0: err, 1: bits } = job.run();
  if (err !== undefined)
    throw err;

  return bits;
}

async function hkdfDeriveBits(algorithm, baseKey, length) {
  validateUint32(length, 'length');
  const { hash } = algorithm;
  const salt = getArrayBufferOrView(algorithm.salt, 'algorithm.salt');
  const info = getArrayBufferOrView(algorithm.info, 'algorithm.info');
  if (hash === undefined)
    throw new ERR_MISSING_OPTION('algorithm.hash');

  let byteLength = 512 / 8;
  if (length !== undefined) {
    if (length === 0)
      throw lazyDOMException('length cannot be zero', 'OperationError');
    if (length % 8) {
      throw lazyDOMException(
        'length must be a multiple of 8',
        'OperationError');
    }
    byteLength = length / 8;
  }

  return new Promise((resolve, reject) => {
    hkdf(
      normalizeHashName(hash.name),
      baseKey[kKeyObject],
      salt,
      info,
      byteLength,
      (err, bits) => {
        if (err) return reject(err);
        resolve(bits);
      });
  });
}

module.exports = {
  hkdf,
  hkdfSync,
  hkdfDeriveBits,
};
