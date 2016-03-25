// Note: In 0.8 and before, crypto functions all defaulted to using
// binary-encoded strings rather than buffers.

'use strict';

const internalUtil = require('internal/util');
internalUtil.assertCrypto(exports);

exports.DEFAULT_ENCODING = 'buffer';

const binding = process.binding('crypto');
const randomBytes = binding.randomBytes;
const getCiphers = binding.getCiphers;
const getHashes = binding.getHashes;
const getCurves = binding.getCurves;
const getFipsCrypto = binding.getFipsCrypto;
const setFipsCrypto = binding.setFipsCrypto;

const constants = require('constants');
const crypto_util = require('internal/crypto/util');
const filterDuplicates = crypto_util.filterDuplicates;

require('internal/crypto/hashbase')(binding, exports);
require('internal/crypto/cipherbase')(binding, exports);
require('internal/crypto/signbase')(binding, exports);
require('internal/crypto/dhbase')(binding, exports);
require('internal/crypto/pbkdf2')(binding, exports);
require('internal/crypto/rsa')(binding, exports);

exports.Certificate = require('internal/crypto/certificate');

exports.setEngine = function setEngine(id, flags) {
  if (typeof id !== 'string')
    throw new TypeError('"id" argument should be a string');

  if (flags && typeof flags !== 'number')
    throw new TypeError('"flags" argument should be a number, if present');
  flags >>>= 0;

  // Use provided engine for everything by default
  if (flags === 0)
    flags = constants.ENGINE_METHOD_ALL;

  return binding.setEngine(id, flags);
};

exports.randomBytes = exports.pseudoRandomBytes = randomBytes;

exports.rng = exports.prng = randomBytes;

exports.getCiphers = function() {
  return filterDuplicates(getCiphers());
};


exports.getHashes = function() {
  return filterDuplicates(getHashes());
};


exports.getCurves = function() {
  return filterDuplicates(getCurves());
};

Object.defineProperty(exports, 'fips', {
  get: getFipsCrypto,
  set: setFipsCrypto
});

// Legacy API
exports.__defineGetter__('createCredentials',
  internalUtil.deprecate(function() {
    return require('tls').createSecureContext;
  }, 'crypto.createCredentials is deprecated. ' +
     'Use tls.createSecureContext instead.'));

exports.__defineGetter__('Credentials', internalUtil.deprecate(function() {
  return require('tls').SecureContext;
}, 'crypto.Credentials is deprecated. ' +
   'Use tls.createSecureContext instead.'));
