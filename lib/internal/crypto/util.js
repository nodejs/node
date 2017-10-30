'use strict';

const {
  getCiphers: _getCiphers,
  getCurves: _getCurves,
  getHashes: _getHashes,
  setEngine: _setEngine,
  timingSafeEqual: _timingSafeEqual
} = process.binding('crypto');

const {
  ENGINE_METHOD_ALL
} = process.binding('constants').crypto;

const errors = require('internal/errors');
const { Buffer } = require('buffer');
const {
  cachedResult,
  filterDuplicateStrings
} = require('internal/util');
const {
  isArrayBufferView
} = require('internal/util/types');

var defaultEncoding = 'buffer';

function setDefaultEncoding(val) {
  defaultEncoding = val;
}

function getDefaultEncoding() {
  return defaultEncoding;
}

// This is here because many functions accepted binary strings without
// any explicit encoding in older versions of node, and we don't want
// to break them unnecessarily.
function toBuf(str, encoding) {
  if (typeof str === 'string') {
    if (encoding === 'buffer' || !encoding)
      encoding = 'utf8';
    return Buffer.from(str, encoding);
  }
  return str;
}

const getCiphers = cachedResult(() => filterDuplicateStrings(_getCiphers()));
const getHashes = cachedResult(() => filterDuplicateStrings(_getHashes()));
const getCurves = cachedResult(() => filterDuplicateStrings(_getCurves()));

function setEngine(id, flags) {
  if (typeof id !== 'string')
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'id', 'string');

  if (flags && typeof flags !== 'number')
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'flags', 'number');
  flags = flags >>> 0;

  // Use provided engine for everything by default
  if (flags === 0)
    flags = ENGINE_METHOD_ALL;

  if (!_setEngine(id, flags))
    throw new errors.Error('ERR_CRYPTO_ENGINE_UNKNOWN', id);
}

function timingSafeEqual(a, b) {
  if (!isArrayBufferView(a)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'a',
                               ['Buffer', 'TypedArray', 'DataView']);
  }
  if (!isArrayBufferView(b)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'b',
                               ['Buffer', 'TypedArray', 'DataView']);
  }
  if (a.length !== b.length) {
    throw new errors.RangeError('ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH');
  }
  return _timingSafeEqual(a, b);
}

module.exports = {
  getCiphers,
  getCurves,
  getDefaultEncoding,
  getHashes,
  setDefaultEncoding,
  setEngine,
  timingSafeEqual,
  toBuf
};
