'use strict';

// Low-level binary helpers: bounds-checked archive ranges, safe 64-bit
// integer read/write, and buffer coercion of user input.

const {
  BigInt,
  Number,
  NumberIsInteger,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_ZIP_INVALID_ARCHIVE,
  },
} = require('internal/errors');
const {
  isAnyArrayBuffer,
  isArrayBufferView,
  isUint8Array,
} = require('internal/util/types');
const { Buffer } = require('buffer');
const { BIGINT_MAX_SAFE_INTEGER } = require('internal/zip/constants');

// Reject an [offset, offset + length) slice that escapes the archive buffer
// before it is used to read a record: guards every offset taken from archive
// bytes against corrupt or hostile values.
function validateArchiveRange(buffer, offset, length, what) {
  if (
    !NumberIsInteger(offset) ||
    offset < 0 ||
    !NumberIsInteger(length) ||
    length < 0 ||
    offset + length > buffer.length
  ) {
    throw new ERR_ZIP_INVALID_ARCHIVE(`${what} is out of bounds`);
  }
}

// Read a little-endian u64 (sizes/offsets, Zip64) that must land in the JS
// safe-integer range; a field past the buffer or beyond that range means a
// corrupt or hostile archive.
function readSafeUint64(buffer, offset) {
  if (offset + 8 > buffer.length) {
    throw new ERR_ZIP_INVALID_ARCHIVE('64-bit field is out of bounds');
  }
  const value = buffer.readBigUInt64LE(offset);
  if (value > BIGINT_MAX_SAFE_INTEGER) {
    throw new ERR_ZIP_INVALID_ARCHIVE('64-bit field exceeds the safe integer range');
  }
  return Number(value);
}

// Write a JS number as a little-endian u64; the write paths only feed values
// already bounded by the safe-integer range, so no range check is needed here.
function writeSafeUint64(buffer, offset, value) {
  buffer.writeBigUInt64LE(BigInt(value), offset);
}


// Coerce user-supplied binary input to a Buffer, aliasing the same memory
// (no copy) for a TypedArray/DataView/ArrayBuffer and rejecting anything else.
// internal/crypto/util.js's getArrayBufferOrView() covers similar coercion but
// returns the view unchanged (and accepts strings with an encoding); this
// helper exists because the ZIP code needs an actual Buffer over that memory.
function toBuffer(value, name) {
  if (isUint8Array(value)) {
    return Buffer.isBuffer(value) ?
      value : Buffer.from(value.buffer, value.byteOffset, value.byteLength);
  }
  if (isArrayBufferView(value)) {
    return Buffer.from(value.buffer, value.byteOffset, value.byteLength);
  }
  if (isAnyArrayBuffer(value)) {
    return Buffer.from(value);
  }
  throw new ERR_INVALID_ARG_TYPE(
    name, ['Buffer', 'TypedArray', 'DataView', 'ArrayBuffer'], value);
}

module.exports = {
  validateArchiveRange,
  readSafeUint64,
  writeSafeUint64,
  toBuffer,
};
