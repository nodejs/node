'use strict';

const {
  Symbol,
} = primordials;

const {
  getCiphers: _getCiphers,
  getCurves: _getCurves,
  getHashes: _getHashes,
  setEngine: _setEngine,
  timingSafeEqual: _timingSafeEqual
} = internalBinding('crypto');

const {
  ENGINE_METHOD_ALL
} = internalBinding('constants').crypto;

const {
  hideStackFrames,
  codes: {
    ERR_CRYPTO_ENGINE_UNKNOWN,
    ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH,
    ERR_INVALID_ARG_TYPE,
  }
} = require('internal/errors');
const { validateString } = require('internal/validators');
const { Buffer } = require('buffer');
const {
  cachedResult,
  filterDuplicateStrings
} = require('internal/util');
const {
  isArrayBufferView
} = require('internal/util/types');

const kHandle = Symbol('kHandle');

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
function toBuf(val, encoding) {
  if (typeof val === 'string') {
    if (encoding === 'buffer')
      encoding = 'utf8';
    return Buffer.from(val, encoding);
  }
  return val;
}

const getCiphers = cachedResult(() => filterDuplicateStrings(_getCiphers()));
const getHashes = cachedResult(() => filterDuplicateStrings(_getHashes()));
const getCurves = cachedResult(() => filterDuplicateStrings(_getCurves()));

function setEngine(id, flags) {
  validateString(id, 'id');
  if (flags && typeof flags !== 'number')
    throw new ERR_INVALID_ARG_TYPE('flags', 'number', flags);
  flags = flags >>> 0;

  // Use provided engine for everything by default
  if (flags === 0)
    flags = ENGINE_METHOD_ALL;

  if (!_setEngine(id, flags))
    throw new ERR_CRYPTO_ENGINE_UNKNOWN(id);
}

function timingSafeEqual(buf1, buf2) {
  if (!isArrayBufferView(buf1)) {
    throw new ERR_INVALID_ARG_TYPE('buf1',
                                   ['Buffer', 'TypedArray', 'DataView'], buf1);
  }
  if (!isArrayBufferView(buf2)) {
    throw new ERR_INVALID_ARG_TYPE('buf2',
                                   ['Buffer', 'TypedArray', 'DataView'], buf2);
  }
  if (buf1.byteLength !== buf2.byteLength) {
    throw new ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH();
  }
  return _timingSafeEqual(buf1, buf2);
}

const getArrayBufferView = hideStackFrames((buffer, name, encoding) => {
  if (typeof buffer === 'string') {
    if (encoding === 'buffer')
      encoding = 'utf8';
    return Buffer.from(buffer, encoding);
  }
  if (!isArrayBufferView(buffer)) {
    throw new ERR_INVALID_ARG_TYPE(
      name,
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      buffer
    );
  }
  return buffer;
});

module.exports = {
  getArrayBufferView,
  getCiphers,
  getCurves,
  getDefaultEncoding,
  getHashes,
  kHandle,
  setDefaultEncoding,
  setEngine,
  timingSafeEqual,
  toBuf
};
