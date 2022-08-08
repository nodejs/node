'use strict';

const {
  FunctionPrototypeCall,
  Promise,
} = primordials;

const { Buffer } = require('buffer');

const {
  PBKDF2Job,
  kCryptoJobAsync,
  kCryptoJobSync,
} = internalBinding('crypto');

const {
  validateCallback,
  validateInt32,
  validateInteger,
  validateString,
  validateUint32,
} = require('internal/validators');

const { ERR_MISSING_OPTION } = require('internal/errors').codes;

const {
  getArrayBufferOrView,
  getDefaultEncoding,
  normalizeHashName,
  kKeyObject,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

function pbkdf2(password, salt, iterations, keylen, digest, callback) {
  if (typeof digest === 'function') {
    callback = digest;
    digest = undefined;
  }

  ({ password, salt, iterations, keylen, digest } =
    check(password, salt, iterations, keylen, digest));

  validateCallback(callback);

  const job = new PBKDF2Job(
    kCryptoJobAsync,
    password,
    salt,
    iterations,
    keylen,
    digest);

  const encoding = getDefaultEncoding();
  job.ondone = (err, result) => {
    if (err !== undefined)
      return FunctionPrototypeCall(callback, job, err);
    const buf = Buffer.from(result);
    if (encoding === 'buffer')
      return FunctionPrototypeCall(callback, job, null, buf);
    FunctionPrototypeCall(callback, job, null, buf.toString(encoding));
  };

  job.run();
}

function pbkdf2Sync(password, salt, iterations, keylen, digest) {
  ({ password, salt, iterations, keylen, digest } =
    check(password, salt, iterations, keylen, digest));

  const job = new PBKDF2Job(
    kCryptoJobSync,
    password,
    salt,
    iterations,
    keylen,
    digest);

  const { 0: err, 1: result } = job.run();
  if (err !== undefined)
    throw err;

  const buf = Buffer.from(result);
  const encoding = getDefaultEncoding();
  return encoding === 'buffer' ? buf : buf.toString(encoding);
}

function check(password, salt, iterations, keylen, digest) {
  validateString(digest, 'digest');

  password = getArrayBufferOrView(password, 'password');
  salt = getArrayBufferOrView(salt, 'salt');
  // OpenSSL uses a signed int to represent these values, so we are restricted
  // to the 31-bit range here (which is plenty).
  validateInt32(iterations, 'iterations', 1);
  validateInt32(keylen, 'keylen', 0);

  return { password, salt, iterations, keylen, digest };
}

async function pbkdf2DeriveBits(algorithm, baseKey, length) {
  const { iterations } = algorithm;
  let { hash } = algorithm;
  const salt = getArrayBufferOrView(algorithm.salt, 'algorithm.salt');
  if (hash === undefined)
    throw new ERR_MISSING_OPTION('algorithm.hash');
  validateInteger(iterations, 'algorithm.iterations');
  if (iterations === 0)
    throw lazyDOMException(
      'iterations cannot be zero',
      'OperationError');

  hash = normalizeHashName(hash.name);

  const raw = baseKey[kKeyObject].export();

  let byteLength = 64;  // the default
  if (length !== undefined) {
    if (length === 0)
      throw lazyDOMException('length cannot be zero', 'OperationError');
    if (length === null)
      throw lazyDOMException('length cannot be null', 'OperationError');
    validateUint32(length, 'length');
    if (length % 8) {
      throw lazyDOMException(
        'length must be a multiple of 8',
        'OperationError');
    }
    byteLength = length / 8;
  }

  return new Promise((resolve, reject) => {
    pbkdf2(raw, salt, iterations, byteLength, hash, (err, result) => {
      if (err) return reject(err);
      resolve(result.buffer);
    });
  });
}

module.exports = {
  pbkdf2,
  pbkdf2Sync,
  pbkdf2DeriveBits,
};
