'use strict';

const {
  FunctionPrototypeCall,
  MathPow,
  Uint8Array,
} = primordials;

const { Buffer } = require('buffer');

const {
  Argon2Job,
  kCryptoJobAsync,
  kCryptoJobSync,
} = internalBinding('crypto');

const { getArrayBufferOrView } = require('internal/crypto/util');
const {
  validateFunction,
  validateInteger,
  validateOneOf,
  validateString,
  validateUint32,
} = require('internal/validators');

const {
  codes: {
    ERR_CRYPTO_ARGON2_NOT_SUPPORTED,
  },
} = require('internal/errors');

/**
 * @param {ArrayBufferLike} password
 * @param {ArrayBufferLike} salt
 * @param {number} keylen
 * @param {object} [options]
 * @param {'argon2d' | 'argon2i' | 'argon2id'} [options.algorithm]
 * @param {ArrayBufferLike} [options.secret]
 * @param {ArrayBufferLike} [options.ad]
 * @param {number} [options.iter]
 * @param {number} [options.lanes]
 * @param {number} [options.memcost]
 * @param {Function} callback
 */
function argon2(password, salt, keylen, options, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = undefined;
  }
  options = check(password, salt, keylen, options);

  validateFunction(callback, 'callback');

  const { algorithm, secret, ad, iter, lanes, memcost } = options;
  ({ password, salt, keylen } = options);

  const job = new Argon2Job(
    kCryptoJobAsync, password, salt, algorithm, secret, ad, iter, lanes, memcost, keylen);

  job.ondone = (error, result) => {
    if (error !== undefined)
      return FunctionPrototypeCall(callback, job, error);
    const buf = Buffer.from(result);
    return FunctionPrototypeCall(callback, job, null, buf);
  };

  job.run();
}

/**
 * @param {ArrayBufferLike} password
 * @param {ArrayBufferLike} salt
 * @param {number} keylen
 * @param {object} [options]
 * @param {'string'} [options.algorithm]
 * @param {ArrayBufferLike} [options.secret]
 * @param {ArrayBufferLike} [options.ad]
 * @param {number} [options.iter]
 * @param {number} [options.lanes]
 * @param {number} [options.memcost]
 * @returns {Buffer}
 */
function argon2Sync(password, salt, keylen, options) {
  options = check(password, salt, keylen, options);
  const { algorithm, secret, ad, iter, lanes, memcost } = options;
  ({ password, salt, keylen } = options);
  const job = new Argon2Job(
    kCryptoJobSync,
    password,
    salt,
    algorithm,
    secret,
    ad,
    iter,
    lanes,
    memcost,
    keylen);
  const { 0: err, 1: result } = job.run();

  if (err !== undefined)
    throw err;

  return Buffer.from(result);
}

/**
 * @param {ArrayBufferLike} password
 * @param {ArrayBufferLike} salt
 * @param {number} keylen
 * @param {object} options
 * @param {string} [options.algorithm]
 * @param {ArrayBufferLike} [options.secret]
 * @param {ArrayBufferLike} [options.ad]
 * @param {number} [options.iter]
 * @param {number} [options.lanes]
 * @param {number} [options.memcost]
 * @returns {object}
 */
function check(password, salt, keylen, options) {
  if (Argon2Job === undefined)
    throw new ERR_CRYPTO_ARGON2_NOT_SUPPORTED();

  const MAX_POSITIVE_UINT_32 = MathPow(2, 32) - 1;
  password = getArrayBufferOrView(password, 'password');

  salt = getArrayBufferOrView(salt, 'salt');
  if (salt.byteLength < 8 || salt.byteLength > MAX_POSITIVE_UINT_32) {
    validateInteger(salt.byteLength, 'salt.byteLength', 8, MAX_POSITIVE_UINT_32);
  }

  validateInteger(keylen, 'keylen', 4, MAX_POSITIVE_UINT_32);

  const algorithm = options?.algorithm ?? 'argon2id';
  validateString(algorithm, 'options.algorithm');
  validateOneOf(algorithm,
                'options.algorithm', ['argon2d', 'argon2i', 'argon2id']);

  const secret = options?.secret ? getArrayBufferOrView(options.secret) : new Uint8Array(0);
  const ad = options?.ad ? getArrayBufferOrView(options.ad) : new Uint8Array(0);

  const iter = options?.iter ?? 3;
  validateUint32(iter, 'options.iter', true);

  const lanes = options?.lanes ?? 4;
  validateInteger(lanes, 'options.lanes', 1, (2 ** 24) - 1);

  const memcost = options?.memcost ?? 64 << 10;  // 64Ki * 1 KiB = 64 MiB
  validateInteger(memcost, 'options.memcost', 8 * lanes, MAX_POSITIVE_UINT_32);

  return { password, salt, algorithm, secret, ad, keylen, iter, lanes, memcost };
}

module.exports = {
  argon2,
  argon2Sync,
};
