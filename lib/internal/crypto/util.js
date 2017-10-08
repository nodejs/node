'use strict';

const {
  getCiphers: _getCiphers,
  getCurves: _getCurves,
  getHashes: _getHashes,
  setEngine: _setEngine
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

  return _setEngine(id, flags);
}

module.exports = {
  getCiphers,
  getCurves,
  getDefaultEncoding,
  getHashes,
  setDefaultEncoding,
  setEngine,
  toBuf
};
