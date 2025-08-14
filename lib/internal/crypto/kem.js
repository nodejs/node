'use strict';

const {
  FunctionPrototypeCall,
} = primordials;

const {
  codes: {
    ERR_CRYPTO_KEM_NOT_SUPPORTED,
    ERR_CRYPTO_KEY_REQUIRED,
  },
} = require('internal/errors');

const {
  validateFunction,
} = require('internal/validators');

const {
  kCryptoJobAsync,
  kCryptoJobSync,
  KEMDecapsulateJob,
  KEMEncapsulateJob,
} = internalBinding('crypto');

const {
  preparePrivateKey,
  preparePublicOrPrivateKey,
} = require('internal/crypto/keys');

const {
  getArrayBufferOrView,
} = require('internal/crypto/util');

function encapsulate(key, callback) {
  if (!KEMEncapsulateJob)
    throw new ERR_CRYPTO_KEM_NOT_SUPPORTED();

  if (!key)
    throw new ERR_CRYPTO_KEY_REQUIRED();

  if (callback !== undefined)
    validateFunction(callback, 'callback');

  const {
    data: keyData,
    format: keyFormat,
    type: keyType,
    passphrase: keyPassphrase,
  } = preparePublicOrPrivateKey(key);

  const job = new KEMEncapsulateJob(
    callback ? kCryptoJobAsync : kCryptoJobSync,
    keyData,
    keyFormat,
    keyType,
    keyPassphrase);

  if (!callback) {
    const { 0: err, 1: result } = job.run();
    if (err !== undefined)
      throw err;
    const { 0: sharedKey, 1: ciphertext } = result;
    return { sharedKey, ciphertext };
  }

  job.ondone = (error, result) => {
    if (error) return FunctionPrototypeCall(callback, job, error);
    const { 0: sharedKey, 1: ciphertext } = result;
    FunctionPrototypeCall(callback, job, null, { sharedKey, ciphertext });
  };
  job.run();
}

function decapsulate(key, ciphertext, callback) {
  if (!KEMDecapsulateJob)
    throw new ERR_CRYPTO_KEM_NOT_SUPPORTED();

  if (!key)
    throw new ERR_CRYPTO_KEY_REQUIRED();

  if (callback !== undefined)
    validateFunction(callback, 'callback');

  ciphertext = getArrayBufferOrView(ciphertext, 'ciphertext');

  const {
    data: keyData,
    format: keyFormat,
    type: keyType,
    passphrase: keyPassphrase,
  } = preparePrivateKey(key);

  const job = new KEMDecapsulateJob(
    callback ? kCryptoJobAsync : kCryptoJobSync,
    keyData,
    keyFormat,
    keyType,
    keyPassphrase,
    ciphertext);

  if (!callback) {
    const { 0: err, 1: result } = job.run();
    if (err !== undefined)
      throw err;

    return result;
  }

  job.ondone = (error, result) => {
    if (error) return FunctionPrototypeCall(callback, job, error);
    FunctionPrototypeCall(callback, job, null, result);
  };
  job.run();
}

module.exports = {
  encapsulate,
  decapsulate,
};
