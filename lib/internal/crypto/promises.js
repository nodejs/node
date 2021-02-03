'use strict';

const {
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
} = primordials;

const {
  HashJob,
  HmacJob,
  kCryptoJobAsync,
  kSigEncDER,
  kSigEncP1363,
  kSignJobModeSign,
  kSignJobModeVerify,
  SignJob,
} = internalBinding('crypto');

const {
  crypto: constants,
} = internalBinding('constants');

const {
  asyncDiffieHellman,
  dhEnabledKeyTypes,
} = require('internal/crypto/diffiehellman');

const {
  createPublicKey,
  isCryptoKey,
  isKeyObject,
} = require('internal/crypto/keys');

const {
  jobPromise,
  kHandle,
  kKeyObject,
} = require('internal/crypto/util');

const {
  hideStackFrames,
  codes: {
    ERR_CRYPTO_INCOMPATIBLE_KEY,
    ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  }
} = require('internal/errors');

const {
  isArrayBufferView,
} = require('internal/util/types');

const {
  validateObject,
  validateString,
  validateUint32,
} = require('internal/validators');

const { Buffer } = require('buffer');

const validateKeyType = hideStackFrames((value, oneOf) => {
  if (!ArrayPrototypeIncludes(oneOf, value)) {
    const allowed = ArrayPrototypeJoin(oneOf, ' or ');
    throw new ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE(value, allowed);
  }
});

function getIntOption(name, options) {
  if (typeof options === 'object') {
    const value = options[name];
    if (value !== undefined) {
      if (value === value >> 0) {
        return value;
      }
      throw new ERR_INVALID_ARG_VALUE(`options.${name}`, value);
    }
  }
  return undefined;
}

function getPadding(options) {
  return getIntOption('padding', options);
}

function getSaltLength(options) {
  return getIntOption('saltLength', options);
}

function getOutputLength(options) {
  if (typeof options === 'object') {
    if (options.outputLength !== undefined) {
      validateUint32(options.outputLength, 'options.outputLength');
      return options.outputLength << 3;
    }
  }
}

function getDSASignatureEncoding(options) {
  if (typeof options === 'object') {
    const { dsaEncoding = 'der' } = options;
    if (dsaEncoding === 'der')
      return kSigEncDER;
    else if (dsaEncoding === 'ieee-p1363')
      return kSigEncP1363;
    throw new ERR_INVALID_ARG_VALUE('options.dsaEncoding', dsaEncoding);
  }

  return kSigEncDER;
}

function getKeyObject(key, argument, ...types) {
  if (isCryptoKey(key))
    key = key[kKeyObject];

  if (!isKeyObject(key))
    throw new ERR_INVALID_ARG_TYPE(argument, ['KeyObject', 'CryptoKey'], key);

  validateKeyType(key.type, types);

  return key;
}

function getArrayBufferView(data, argument) {
  if (!isArrayBufferView(data)) {
    throw new ERR_INVALID_ARG_TYPE(
      argument,
      ['Buffer', 'TypedArray', 'DataView'],
      data
    );
  }

  return data;
}

function getData(data) {
  return getArrayBufferView(data, 'data');
}

function getSignature(data) {
  return getArrayBufferView(data, 'signature');
}

async function digest(algorithm, data, options) {
  validateString(algorithm, 'algorithm');

  data = getData(data);

  const outputLength = getOutputLength(options);

  const digest = await jobPromise(new HashJob(
    kCryptoJobAsync,
    algorithm,
    data,
    outputLength));

  return Buffer.from(digest);
}

async function hmac(algorithm, data, key) {
  validateString(algorithm, 'algorithm');

  data = getData(data);

  const keyObject = getKeyObject(key, 'key', 'secret');

  const digest = await jobPromise(new HmacJob(
    kCryptoJobAsync,
    kSignJobModeSign,
    algorithm,
    keyObject[kHandle],
    data));

  return Buffer.from(digest);
}

async function sign(algorithm, data, key, options) {
  if (algorithm != null)
    validateString(algorithm, 'algorithm');

  data = getData(data);

  const keyObject = getKeyObject(key, 'key', 'private');

  // Options specific to RSA
  const rsaPadding = getPadding(options);
  const pssSaltLength = getSaltLength(options);

  // Options specific to (EC)DSA
  // TODO(@jasnell): add dsaSigEnc to SignJob
  const dsaSigEnc = getDSASignatureEncoding(options);

  const signature = await jobPromise(new SignJob(
    kCryptoJobAsync,
    kSignJobModeSign,
    keyObject[kHandle],
    data,
    algorithm,
    pssSaltLength,
    rsaPadding));

  return Buffer.from(signature);
}

async function verify(algorithm, data, key, signature, options) {
  if (algorithm != null)
    validateString(algorithm, 'algorithm');

  data = getData(data);

  let keyObject = getKeyObject(key, 'key', 'public', 'private');

  if (keyObject.type === 'private')
    keyObject = createPublicKey(keyObject);

  signature = getSignature(signature);

  // Options specific to RSA
  const rsaPadding = getPadding(options);
  const pssSaltLength = getSaltLength(options);

  // Options specific to (EC)DSA
  // TODO(@jasnell): add dsaSigEnc to SignJob
  const dsaSigEnc = getDSASignatureEncoding(options);

  return jobPromise(new SignJob(
    kCryptoJobAsync,
    kSignJobModeVerify,
    keyObject[kHandle],
    data,
    algorithm,
    pssSaltLength,
    rsaPadding,
    signature));
}

async function diffieHellman(options) {
  validateObject(options, 'options');

  const privateKey = getKeyObject(options.privateKey,
                                  'options.privateKey', 'private');
  const publicKey = getKeyObject(options.publicKey,
                                 'options.publicKey', 'public');

  const privateType = privateKey.asymmetricKeyType;
  const publicType = publicKey.asymmetricKeyType;
  if (privateType !== publicType || !dhEnabledKeyTypes.has(privateType)) {
    throw new ERR_CRYPTO_INCOMPATIBLE_KEY('key types for Diffie-Hellman',
                                          `${privateType} and ${publicType}`);
  }

  const bits = await asyncDiffieHellman(publicKey, privateKey);

  return Buffer.from(bits);
}

module.exports = {
  exports: {
    diffieHellman,
    digest,
    hmac,
    sign,
    verify,
    constants,
  }
};
