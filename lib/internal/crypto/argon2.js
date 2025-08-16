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
  kTypeArgon2d,
  kTypeArgon2i,
  kTypeArgon2id,
} = internalBinding('crypto');

const { getArrayBufferOrView } = require('internal/crypto/util');
const {
  validateString,
  validateFunction,
  validateInteger,
  validateObject,
  validateOneOf,
  validateUint32,
} = require('internal/validators');

const {
  codes: {
    ERR_CRYPTO_ARGON2_NOT_SUPPORTED,
  },
} = require('internal/errors');

/**
 * @param {'argon2d' | 'argon2i' | 'argon2id'} algorithm
 * @param {object} parameters
 * @param {ArrayBufferLike} parameters.message
 * @param {ArrayBufferLike} parameters.nonce
 * @param {number} parameters.parallelism
 * @param {number} parameters.tagLength
 * @param {number} parameters.memory
 * @param {number} parameters.passes
 * @param {ArrayBufferLike} [parameters.secret]
 * @param {ArrayBufferLike} [parameters.associatedData]
 * @param {Function} callback
 */
function argon2(algorithm, parameters, callback) {
  parameters = check(algorithm, parameters);

  validateFunction(callback, 'callback');

  const job = new Argon2Job(
    kCryptoJobAsync,
    parameters.message,
    parameters.nonce,
    parameters.parallelism,
    parameters.tagLength,
    parameters.memory,
    parameters.passes,
    parameters.secret,
    parameters.associatedData,
    parameters.type);

  job.ondone = (error, result) => {
    if (error !== undefined)
      return FunctionPrototypeCall(callback, job, error);
    const buf = Buffer.from(result);
    return FunctionPrototypeCall(callback, job, null, buf);
  };

  job.run();
}

/**
 * @param {'argon2d' | 'argon2i' | 'argon2id'} algorithm
 * @param {object} parameters
 * @param {ArrayBufferLike} parameters.message
 * @param {ArrayBufferLike} parameters.nonce
 * @param {number} parameters.parallelism
 * @param {number} parameters.tagLength
 * @param {number} parameters.memory
 * @param {number} parameters.passes
 * @param {ArrayBufferLike} [parameters.secret]
 * @param {ArrayBufferLike} [parameters.associatedData]
 * @returns {Buffer}
 */
function argon2Sync(algorithm, parameters) {
  parameters = check(algorithm, parameters);

  const job = new Argon2Job(
    kCryptoJobSync,
    parameters.message,
    parameters.nonce,
    parameters.parallelism,
    parameters.tagLength,
    parameters.memory,
    parameters.passes,
    parameters.secret,
    parameters.associatedData,
    parameters.type);

  const { 0: err, 1: result } = job.run();

  if (err !== undefined)
    throw err;

  return Buffer.from(result);
}

/**
 * @param {'argon2d' | 'argon2i' | 'argon2id'} algorithm
 * @param {object} parameters
 * @param {ArrayBufferLike} parameters.message
 * @param {ArrayBufferLike} parameters.nonce
 * @param {number} parameters.parallelism
 * @param {number} parameters.tagLength
 * @param {number} parameters.memory
 * @param {number} parameters.passes
 * @param {ArrayBufferLike} [parameters.secret]
 * @param {ArrayBufferLike} [parameters.associatedData]
 * @returns {object}
 */
function check(algorithm, parameters) {
  if (Argon2Job === undefined)
    throw new ERR_CRYPTO_ARGON2_NOT_SUPPORTED();

  validateString(algorithm, 'algorithm');
  validateOneOf(algorithm, 'algorithm', ['argon2d', 'argon2i', 'argon2id']);

  let type;
  switch (algorithm) {
    case 'argon2d':
      type = kTypeArgon2d;
      break;
    case 'argon2i':
      type = kTypeArgon2i;
      break;
    case 'argon2id':
      type = kTypeArgon2id;
      break;
    default:  // unreachable
      throw new ERR_CRYPTO_ARGON2_NOT_SUPPORTED();
  }

  validateObject(parameters, 'parameters');

  const { parallelism, tagLength, memory, passes } = parameters;
  const MAX_POSITIVE_UINT_32 = MathPow(2, 32) - 1;

  const message = getArrayBufferOrView(parameters.message, 'parameters.message');
  validateInteger(message.byteLength, 'parameters.message.byteLength', 0, MAX_POSITIVE_UINT_32);

  const nonce = getArrayBufferOrView(parameters.nonce, 'parameters.nonce');
  validateInteger(nonce.byteLength, 'parameters.nonce.byteLength', 8, MAX_POSITIVE_UINT_32);

  validateInteger(parallelism, 'parameters.parallelism', 1, MathPow(2, 24) - 1);
  validateInteger(tagLength, 'parameters.tagLength', 4, MAX_POSITIVE_UINT_32);
  validateInteger(memory, 'parameters.memory', 8 * parallelism, MAX_POSITIVE_UINT_32);
  validateUint32(passes, 'parameters.passes', true);

  let secret;
  if (parameters.secret === undefined) {
    secret = new Uint8Array(0);
  } else {
    secret = getArrayBufferOrView(parameters.secret);
    validateInteger(secret.byteLength, 'parameters.secret.byteLength', 0, MAX_POSITIVE_UINT_32);
  }

  let associatedData;
  if (parameters.associatedData === undefined) {
    associatedData = new Uint8Array(0);
  } else {
    associatedData = getArrayBufferOrView(parameters.associatedData);
    validateInteger(associatedData.byteLength, 'parameters.associatedData.byteLength', 0, MAX_POSITIVE_UINT_32);
  }

  return { message, nonce, secret, associatedData, tagLength, passes, parallelism, memory, type };
}

module.exports = {
  argon2,
  argon2Sync,
};
