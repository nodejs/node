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
  validateObject,
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
 * @param {object} options
 * @param {ArrayBufferLike} options.message
 * @param {ArrayBufferLike} options.nonce
 * @param {number} options.parallelism
 * @param {number} options.tagLength
 * @param {number} options.memory
 * @param {number} options.passes
 * @param {ArrayBufferLike} [options.secret]
 * @param {ArrayBufferLike} [options.associatedData]
 * @param {'argon2d' | 'argon2i' | 'argon2id'} options.type
 * @param {Function} callback
 */
function argon2(options, callback) {
  validateFunction(callback, 'callback');

  const { message, nonce, parallelism, tagLength, memory, passes, secret, associatedData, type } = check(options);

  const job = new Argon2Job(
    kCryptoJobAsync,
    message,
    nonce,
    parallelism,
    tagLength,
    memory,
    passes,
    secret,
    associatedData,
    type);

  job.ondone = (error, result) => {
    if (error !== undefined)
      return FunctionPrototypeCall(callback, job, error);
    const buf = Buffer.from(result);
    return FunctionPrototypeCall(callback, job, null, buf);
  };

  job.run();
}

/**
 * @param {object} options
 * @param {ArrayBufferLike} options.message
 * @param {ArrayBufferLike} options.nonce
 * @param {number} options.parallelism
 * @param {number} options.tagLength
 * @param {number} options.memory
 * @param {number} options.passes
 * @param {ArrayBufferLike} [options.secret]
 * @param {ArrayBufferLike} [options.associatedData]
 * @param {'argon2d' | 'argon2i' | 'argon2id'} options.type
 * @returns {Buffer}
 */
function argon2Sync(options) {
  const { message, nonce, parallelism, tagLength, memory, passes, secret, associatedData, type } = check(options);

  const job = new Argon2Job(
    kCryptoJobSync,
    message,
    nonce,
    parallelism,
    tagLength,
    memory,
    passes,
    secret,
    associatedData,
    type);

  const { 0: err, 1: result } = job.run();

  if (err !== undefined)
    throw err;

  return Buffer.from(result);
}

/**
 * @param {object} options
 * @param {ArrayBufferLike} options.message
 * @param {ArrayBufferLike} options.nonce
 * @param {number} options.parallelism
 * @param {number} options.tagLength
 * @param {number} options.memory
 * @param {number} options.passes
 * @param {ArrayBufferLike} [options.secret]
 * @param {ArrayBufferLike} [options.associatedData]
 * @param {'argon2d' | 'argon2i' | 'argon2id'} options.type
 * @returns {object}
 */
function check(options) {
  if (Argon2Job === undefined)
    throw new ERR_CRYPTO_ARGON2_NOT_SUPPORTED();

  validateObject(options, 'options');

  const { parallelism, tagLength, memory, passes, type } = options;
  const MAX_POSITIVE_UINT_32 = MathPow(2, 32) - 1;

  const message = getArrayBufferOrView(options.message, 'message');
  if (message.byteLength > MAX_POSITIVE_UINT_32) {
    validateInteger(message.byteLength, 'message.byteLength', 0, MAX_POSITIVE_UINT_32);
  }

  const nonce = getArrayBufferOrView(options.nonce, 'nonce');
  if (nonce.byteLength < 8 || nonce.byteLength > MAX_POSITIVE_UINT_32) {
    validateInteger(nonce.byteLength, 'nonce.byteLength', 8, MAX_POSITIVE_UINT_32);
  }

  validateInteger(parallelism, 'parallelism', 1, MathPow(2, 24) - 1);
  validateInteger(tagLength, 'tagLength', 4, MAX_POSITIVE_UINT_32);
  validateInteger(memory, 'memory', 8 * parallelism, MAX_POSITIVE_UINT_32);
  validateUint32(passes, 'passes', true);

  const secret = options.secret ? getArrayBufferOrView(options.secret) : new Uint8Array(0);
  if (secret.byteLength > MAX_POSITIVE_UINT_32) {
    validateInteger(secret.byteLength, 'secret.byteLength', 0, MAX_POSITIVE_UINT_32);
  }

  const associatedData = options.associatedData ? getArrayBufferOrView(options.associatedData) : new Uint8Array(0);
  if (associatedData.byteLength > MAX_POSITIVE_UINT_32) {
    validateInteger(associatedData.byteLength, 'associatedData.byteLength', 0, MAX_POSITIVE_UINT_32);
  }

  validateString(type, 'type');
  validateOneOf(type, 'type', ['argon2d', 'argon2i', 'argon2id']);

  return { message, nonce, type, secret, associatedData, tagLength, passes, parallelism, memory };
}

module.exports = {
  argon2,
  argon2Sync,
};
