'use strict';

const {
  FunctionPrototypeCall,
  Promise,
} = primordials;

const { Buffer } = require('buffer');

const {
  ScryptJob,
  kCryptoJobAsync,
  kCryptoJobSync,
} = internalBinding('crypto');

const {
  validateCallback,
  validateInteger,
  validateInt32,
  validateUint32,
} = require('internal/validators');

const {
  codes: {
    ERR_CRYPTO_SCRYPT_INVALID_PARAMETER,
    ERR_CRYPTO_SCRYPT_NOT_SUPPORTED,
  }
} = require('internal/errors');

const {
  getArrayBufferOrView,
  getDefaultEncoding,
  kKeyObject,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

const defaults = {
  N: 16384,
  r: 8,
  p: 1,
  maxmem: 32 << 20,  // 32 MB, matches SCRYPT_MAX_MEM.
};

function scrypt(password, salt, keylen, options, callback = defaults) {
  if (callback === defaults) {
    callback = options;
    options = defaults;
  }

  options = check(password, salt, keylen, options);
  const { N, r, p, maxmem } = options;
  ({ password, salt, keylen } = options);

  validateCallback(callback);

  const job = new ScryptJob(
    kCryptoJobAsync, password, salt, N, r, p, maxmem, keylen);

  const encoding = getDefaultEncoding();
  job.ondone = (error, result) => {
    if (error !== undefined)
      return FunctionPrototypeCall(callback, job, error);
    const buf = Buffer.from(result);
    if (encoding === 'buffer')
      return FunctionPrototypeCall(callback, job, null, buf);
    FunctionPrototypeCall(callback, job, null, buf.toString(encoding));
  };

  job.run();
}

function scryptSync(password, salt, keylen, options = defaults) {
  options = check(password, salt, keylen, options);
  const { N, r, p, maxmem } = options;
  ({ password, salt, keylen } = options);
  const job = new ScryptJob(
    kCryptoJobSync, password, salt, N, r, p, maxmem, keylen);
  const { 0: err, 1: result } = job.run();

  if (err !== undefined)
    throw err;

  const buf = Buffer.from(result);
  const encoding = getDefaultEncoding();
  return encoding === 'buffer' ? buf : buf.toString(encoding);
}

function check(password, salt, keylen, options) {
  if (ScryptJob === undefined)
    throw new ERR_CRYPTO_SCRYPT_NOT_SUPPORTED();

  password = getArrayBufferOrView(password, 'password');
  salt = getArrayBufferOrView(salt, 'salt');
  validateInt32(keylen, 'keylen', 0);

  let { N, r, p, maxmem } = defaults;
  if (options && options !== defaults) {
    let has_N, has_r, has_p;
    if (has_N = (options.N !== undefined)) {
      N = options.N;
      validateUint32(N, 'N');
    }
    if (options.cost !== undefined) {
      if (has_N) throw new ERR_CRYPTO_SCRYPT_INVALID_PARAMETER();
      N = options.cost;
      validateUint32(N, 'cost');
    }
    if (has_r = (options.r !== undefined)) {
      r = options.r;
      validateUint32(r, 'r');
    }
    if (options.blockSize !== undefined) {
      if (has_r) throw new ERR_CRYPTO_SCRYPT_INVALID_PARAMETER();
      r = options.blockSize;
      validateUint32(r, 'blockSize');
    }
    if (has_p = (options.p !== undefined)) {
      p = options.p;
      validateUint32(p, 'p');
    }
    if (options.parallelization !== undefined) {
      if (has_p) throw new ERR_CRYPTO_SCRYPT_INVALID_PARAMETER();
      p = options.parallelization;
      validateUint32(p, 'parallelization');
    }
    if (options.maxmem !== undefined) {
      maxmem = options.maxmem;
      validateInteger(maxmem, 'maxmem', 0);
    }
    if (N === 0) N = defaults.N;
    if (r === 0) r = defaults.r;
    if (p === 0) p = defaults.p;
    if (maxmem === 0) maxmem = defaults.maxmem;
  }

  return { password, salt, keylen, N, r, p, maxmem };
}

async function scryptDeriveBits(algorithm, baseKey, length) {
  validateUint32(length, 'length');
  const {
    N = 16384,
    r = 8,
    p = 1,
    maxmem = 32 * 1024 * 1024,
    encoding,
  } = algorithm;
  validateUint32(N, 'algorithm.N');
  validateUint32(r, 'algorithm.r');
  validateUint32(p, 'algorithm.p');
  validateUint32(maxmem, 'algorithm.maxmem');
  const salt = getArrayBufferOrView(algorithm.salt, 'algorithm.salt', encoding);

  const raw = baseKey[kKeyObject].export();

  let byteLength = 64;  // the default
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
    scrypt(raw, salt, byteLength, { N, r, p, maxmem }, (err, result) => {
      if (err) return reject(err);
      resolve(result.buffer);
    });
  });
}

module.exports = {
  scrypt,
  scryptSync,
  scryptDeriveBits,
};
