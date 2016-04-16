// Note: In 0.8 and before, crypto functions all defaulted to using
// binary-encoded strings rather than buffers.

'use strict';

require('internal/util').assertCrypto(exports);

exports.DEFAULT_ENCODING = 'buffer';

const binding = process.binding('crypto');

const constants = require('constants');
const crypto_util = require('internal/crypto/util');

require('internal/crypto/hashbase')(binding, exports);
require('internal/crypto/cipherbase')(binding, exports);
require('internal/crypto/signbase')(binding, exports);
require('internal/crypto/dhbase')(binding, exports);
require('internal/crypto/pbkdf2')(binding, exports);
require('internal/crypto/rsa')(binding, exports);
require('internal/crypto/certificate')(binding, exports);
require('internal/crypto/legacy')(exports);

exports.setEngine = function setEngine(id, flags) {
  if (typeof id !== 'string')
    throw new TypeError('"id" argument must be a string');

  if (flags && typeof flags !== 'number')
    throw new TypeError('"flags" argument must be a number, if present');
  flags >>>= 0;

  // Use provided engine for everything by default
  if (flags === 0)
    flags = constants.ENGINE_METHOD_ALL;

  return binding.setEngine(id, flags);
};

exports.rng = exports.prng =
              exports.randomBytes =
              exports.pseudoRandomBytes = binding.randomBytes;

var _ciphers, _hashes, _curves;

exports.getCiphers = function() {
  if (!_ciphers)
    _ciphers = crypto_util.filterDuplicates(binding.getCiphers());
  return _ciphers;
};

exports.getHashes = function() {
  if (!_hashes)
    _hashes = crypto_util.filterDuplicates(binding.getHashes());
  return _hashes;
};

exports.getCurves = function() {
  if (!_curves)
    _curves = crypto_util.filterDuplicates(binding.getCurves());
  return _curves;
};

Object.defineProperty(exports, 'fips', {
  get: binding.getFipsCrypto,
  set: binding.setFipsCrypto
});
