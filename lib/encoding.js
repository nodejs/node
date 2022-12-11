'use strict';

const {
  isAscii: _isAscii,
  isUtf8: _isUtf8,
  countUtf8: _countUtf8,
} = internalBinding('encoding_methods');

const {
  isUint8Array,
} = require('internal/util/types');

const {
  emitExperimentalWarning,
} = require('internal/util');

const { TextEncoder } = require('util');
const { Buffer } = require('buffer');

const encoder = new TextEncoder();

emitExperimentalWarning('Encoding');

function isAscii(input) {
  if (Buffer.isBuffer(input) || isUint8Array(input)) {
    return _isAscii(input.buffer);
  }

  if (typeof input === 'string') {
    const { buffer } = encoder.encode(input);
    return _isAscii(buffer);
  }

  return false;
}

function isUtf8(input) {
  if (Buffer.isBuffer(input) || isUint8Array(input)) {
    return _isUtf8(input.buffer);
  }

  return false;
}

function countUtf8(input) {
  if (Buffer.isBuffer(input) || isUint8Array(input)) {
    return _countUtf8(input.buffer);
  }

  return 0;
}

module.exports = {
  isAscii,
  isUtf8,
  countUtf8,
};
