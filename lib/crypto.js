// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// Note: In 0.8 and before, crypto functions all defaulted to using
// binary-encoded strings rather than buffers.

'use strict';

const internalUtil = require('internal/util');
internalUtil.assertCrypto();

const binding = process.binding('crypto');
const constants = process.binding('constants').crypto;
const cryptoUtil = require('internal/crypto/util');
const internalCert = require('internal/crypto/certificate');
const internalCipher = require('internal/crypto/cipher');
const internalDh = require('internal/crypto/diffiehellman');
const internalHash = require('internal/crypto/hash');
const internalSign = require('internal/crypto/sign');
const getCiphers = binding.getCiphers;
const getHashes = binding.getHashes;
const getCurves = binding.getCurves;
const getFipsCrypto = binding.getFipsCrypto;
const randomBytes = binding.randomBytes;
const setFipsCrypto = binding.setFipsCrypto;
const timingSafeEqual = binding.timingSafeEqual;

const toBuf = cryptoUtil.toBuf;
const filterDuplicateStrings = internalUtil.filterDuplicateStrings;
const cachedResult = internalUtil.cachedResult;

function rsa(method, defaultPadding) {
  return function(options, buffer) {
    var key = options.key || options;
    var passphrase = options.passphrase || null;
    var padding = options.padding || defaultPadding;
    return method(toBuf(key), buffer, padding, passphrase);
  };
}

module.exports = exports = {
  createCipher: internalCipher.createCipher,
  createCipheriv: internalCipher.createCipheriv,
  createDecipher: internalCipher.createDecipher,
  createDecipheriv: internalCipher.createDecipheriv,
  createDiffieHellman: internalDh.createDiffieHellman,
  createDiffieHellmanGroup: internalDh.createDiffieHellmanGroup,
  createECDH: internalDh.createECDH,
  createHash: internalHash.createHash,
  createHmac: internalHash.createHmac,
  createSign: internalSign.createSign,
  createVerify: internalSign.createVerify,
  getDiffieHellman: internalDh.createDiffieHellmanGroup,
  getCiphers: cachedResult(() => filterDuplicateStrings(getCiphers())),
  getCurves: cachedResult(() => filterDuplicateStrings(getCurves())),
  getHashes: cachedResult(() => filterDuplicateStrings(getHashes())),
  pbkdf2: pbkdf2,
  pbkdf2Sync: pbkdf2Sync,
  publicEncrypt: rsa(binding.publicEncrypt, constants.RSA_PKCS1_OAEP_PADDING),
  publicDecrypt: rsa(binding.publicDecrypt, constants.RSA_PKCS1_PADDING),
  privateEncrypt: rsa(binding.privateEncrypt, constants.RSA_PKCS1_PADDING),
  privateDecrypt: rsa(binding.privateDecrypt, constants.RSA_PKCS1_OAEP_PADDING),
  prng: randomBytes,
  pseudoRandomBytes: randomBytes,
  randomBytes: randomBytes,
  rng: randomBytes,
  setEngine: setEngine,
  timingSafeEqual: timingSafeEqual,
  Certificate: internalCert.createCertificate,
  Cipher: internalCipher.createCipher,
  Cipheriv: internalCipher.createCipheriv,
  Decipher: internalCipher.createDecipher,
  Decipheriv: internalCipher.createDecipheriv,
  DiffieHellman: internalDh.createDiffieHellman,
  DiffieHellmanGroup: internalDh.createDiffieHellmanGroup,
  Hash: internalHash.createHash,
  Hmac: internalHash.createHmac,
  Sign: internalSign.createSign,
  Verify: internalSign.createVerify,

  _toBuf: toBuf
};

Object.defineProperties(exports, {
  'constants': {
    configurable: false,
    enumerable: true,
    value: constants
  },
  'DEFAULT_ENCODING': {
    configurable: true,
    enumerable: true,
    get: function() {
      return cryptoUtil.DEFAULT_ENCODING;
    },
    set: function(encoding) {
      encoding = internalUtil.normalizeEncoding(encoding);
      cryptoUtil.DEFAULT_ENCODING = encoding;
    }
  },
  'fips': {
    get: getFipsCrypto,
    set: setFipsCrypto
  },

  // Legacy API
  'createCredentials': {
    configurable: true,
    enumerable: true,
    get: internalUtil.deprecate(function() {
      return require('tls').createSecureContext;
    }, 'crypto.createCredentials is deprecated. ' +
      'Use tls.createSecureContext instead.', 'DEP0010')
  },
  'Credentials': {
    configurable: true,
    enumerable: true,
    get: internalUtil.deprecate(function() {
      return require('tls').SecureContext;
    }, 'crypto.Credentials is deprecated. ' +
      'Use tls.SecureContext instead.', 'DEP0011')
  }
});

function setEngine(id, flags) {
  if (typeof id !== 'string')
    throw new TypeError('"id" argument should be a string');

  if (flags && typeof flags !== 'number')
    throw new TypeError('"flags" argument should be a number, if present');
  flags = flags >>> 0;

  // Use provided engine for everything by default
  if (flags === 0)
    flags = constants.ENGINE_METHOD_ALL;

  return binding.setEngine(id, flags);
}

function pbkdf2(password, salt, iterations, keylen, digest, callback) {
  if (typeof digest === 'function') {
    callback = digest;
    digest = undefined;
  }

  if (typeof callback !== 'function')
    throw new Error('No callback provided to pbkdf2');

  return _pbkdf2(password, salt, iterations, keylen, digest, callback);
}

function pbkdf2Sync(password, salt, iterations, keylen, digest) {
  return _pbkdf2(password, salt, iterations, keylen, digest);
}

function _pbkdf2(password, salt, iterations, keylen, digest, callback) {

  if (digest === undefined) {
    throw new TypeError(
        'The "digest" argument is required and must not be undefined');
  }

  password = toBuf(password);
  salt = toBuf(salt);

  if (cryptoUtil.DEFAULT_ENCODING === 'buffer')
    return binding.PBKDF2(password, salt, iterations, keylen, digest, callback);

  // at this point, we need to handle encodings.
  var encoding = cryptoUtil.DEFAULT_ENCODING;
  if (callback) {
    var next = function(er, ret) {
      if (ret)
        ret = ret.toString(encoding);
      callback(er, ret);
    };
    binding.PBKDF2(password, salt, iterations, keylen, digest, next);
  } else {
    var ret = binding.PBKDF2(password, salt, iterations, keylen, digest);
    return ret.toString(encoding);
  }
}
