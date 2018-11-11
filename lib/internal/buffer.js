'use strict';

const binding = internalBinding('buffer');
const {
  ERR_BUFFER_OUT_OF_BOUNDS,
  ERR_INVALID_ARG_TYPE,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const {
  validateNumber,
  isUint32
} = require('internal/validators');
const { setupBufferJS } = binding;

// Remove from the binding so that function is only available as exported here.
// (That is, for internal use only.)
delete binding.setupBufferJS;

const kIBDV = Symbol('internal buffer dataview');
const dvWM = new WeakMap();
const getIBDV = (ab) => dvWM.get(ab) ||
  dvWM.set(ab, new DataView(ab)).get(ab);

function checkBounds(buf, offset, byteLength) {
  checkNumberType(offset);
  if (buf.byteLength < offset + byteLength || !isUint32(offset))
    boundsError(offset, buf.length - byteLength);
}

function checkInt(value, min, max, buf, offset, byteLength) {
  if (value > max || value < min) {
    throw new ERR_OUT_OF_RANGE('value', `>= ${min} and <= ${max}`, value);
  }
  checkBounds(buf, offset, byteLength);
}

function checkNumberType(value, type) {
  validateNumber(value, type || 'offset');
}

function boundsError(value, length, type) {
  if (Math.floor(value) !== value) {
    checkNumberType(value, type);
    throw new ERR_OUT_OF_RANGE(type || 'offset', 'an integer', value);
  }

  if (length < 0)
    throw new ERR_BUFFER_OUT_OF_BOUNDS();

  throw new ERR_OUT_OF_RANGE(type || 'offset',
                             `>= ${type ? 1 : 0} and <= ${length}`,
                             value);
}

// Read integers.
function readUIntLE(offset, byteLength) {
  if (offset === undefined)
    throw new ERR_INVALID_ARG_TYPE('offset', 'number', offset);
  if (byteLength === 6)
    return readUInt48LE(this, offset);
  if (byteLength === 5)
    return readUInt40LE(this, offset);
  if (byteLength === 3)
    return readUInt24LE(this, offset);
  if (byteLength === 4)
    return this.readUInt32LE(offset);
  if (byteLength === 2)
    return this.readUInt16LE(offset);
  if (byteLength === 1)
    return this.readUInt8(offset);

  boundsError(byteLength, 6, 'byteLength');
}

function readUInt48LE(buf, offset = 0) {
  checkBounds(buf, offset, 6);

  const upper = buf[kIBDV].getUint16(buf.byteOffset + offset + 4, true);
  const lower = buf[kIBDV].getUint32(buf.byteOffset + offset, true);

  return upper * 2 ** 32 + lower;
}

function readUInt40LE(buf, offset = 0) {
  checkBounds(buf, offset, 5);

  const upper = buf[offset + 4];
  const lower = buf[kIBDV].getUint32(buf.byteOffset + offset, true);

  return upper * 2 ** 32 + lower;
}

function readUInt32LE(offset = 0) {
  checkBounds(this, offset, 4);

  return this[kIBDV].getUint32(this.byteOffset + offset, true);
}

function readUInt24LE(buf, offset = 0) {
  checkBounds(buf, offset, 3);

  const upper = buf[offset + 2];
  const lower = buf[kIBDV].getUint16(buf.byteOffset + offset, true);

  return upper * 2 ** 16 + lower;
}

function readUInt16LE(offset = 0) {
  checkBounds(this, offset, 2);

  return this[kIBDV].getUint16(this.byteOffset + offset, true);
}

function readUInt8(offset = 0) {
  checkBounds(this, offset, 1);

  return this[offset];
}

function readUIntBE(offset, byteLength) {
  if (offset === undefined)
    throw new ERR_INVALID_ARG_TYPE('offset', 'number', offset);
  if (byteLength === 6)
    return readUInt48BE(this, offset);
  if (byteLength === 5)
    return readUInt40BE(this, offset);
  if (byteLength === 3)
    return readUInt24BE(this, offset);
  if (byteLength === 4)
    return this.readUInt32BE(offset);
  if (byteLength === 2)
    return this.readUInt16BE(offset);
  if (byteLength === 1)
    return this.readUInt8(offset);

  boundsError(byteLength, 6, 'byteLength');
}

function readUInt48BE(buf, offset = 0) {
  checkBounds(buf, offset, 6);

  const upper = buf[kIBDV].getUint16(buf.byteOffset + offset);
  const lower = buf[kIBDV].getUint32(buf.byteOffset + offset + 2);

  return upper * 2 ** 32 + lower;
}

function readUInt40BE(buf, offset = 0) {
  checkBounds(buf, offset, 5);

  const upper = buf[offset];
  const lower = buf[kIBDV].getUint32(buf.byteOffset + offset + 1);

  return upper * 2 ** 32 + lower;
}

function readUInt32BE(offset = 0) {
  checkBounds(this, offset, 4);

  return this[kIBDV].getUint32(this.byteOffset + offset);
}

function readUInt24BE(buf, offset = 0) {
  checkBounds(buf, offset, 3);

  const upper = buf[offset];
  const lower = buf[kIBDV].getUint16(buf.byteOffset + offset + 1);

  return upper * 2 ** 16 + lower;
}

function readUInt16BE(offset = 0) {
  checkBounds(this, offset, 2);

  return this[kIBDV].getUint16(this.byteOffset + offset);
}

function readIntLE(offset, byteLength) {
  if (offset === undefined)
    throw new ERR_INVALID_ARG_TYPE('offset', 'number', offset);
  if (byteLength === 6)
    return readInt48LE(this, offset);
  if (byteLength === 5)
    return readInt40LE(this, offset);
  if (byteLength === 3)
    return readInt24LE(this, offset);
  if (byteLength === 4)
    return this.readInt32LE(offset);
  if (byteLength === 2)
    return this.readInt16LE(offset);
  if (byteLength === 1)
    return this.readInt8(offset);

  boundsError(byteLength, 6, 'byteLength');
}

function readInt48LE(buf, offset = 0) {
  checkBounds(buf, offset, 6);

  const val = buf[kIBDV].getUint16(buf.byteOffset + offset + 4, true);
  return (val | (val & 2 ** 15) * 0x1fffe) * 2 ** 32 +
    buf[kIBDV].getUint32(buf.byteOffset + offset, true);
}

function readInt40LE(buf, offset = 0) {
  checkBounds(buf, offset, 5);

  const last = buf[offset + 4];
  return (last | (last & 2 ** 7) * 0x1fffffe) * 2 ** 32 +
    buf[kIBDV].getUint32(buf.byteOffset + offset, true);
}

function readInt32LE(offset = 0) {
  checkBounds(this, offset, 4);

  return this[kIBDV].getInt32(this.byteOffset + offset, true);
}

function readInt24LE(buf, offset = 0) {
  checkBounds(buf, offset, 3);

  const val = buf[kIBDV].getUint16(buf.byteOffset + offset, true) +
    buf[offset + 2] * 2 ** 16;
  return val | (val & 2 ** 23) * 0x1fe;
}

function readInt16LE(offset = 0) {
  checkBounds(this, offset, 2);

  return this[kIBDV].getInt16(this.byteOffset + offset, true);
}

function readInt8(offset = 0) {
  checkBounds(this, offset, 1);

  return this[kIBDV].getInt8(this.byteOffset + offset);
}

function readIntBE(offset, byteLength) {
  if (offset === undefined)
    throw new ERR_INVALID_ARG_TYPE('offset', 'number', offset);
  if (byteLength === 6)
    return readInt48BE(this, offset);
  if (byteLength === 5)
    return readInt40BE(this, offset);
  if (byteLength === 3)
    return readInt24BE(this, offset);
  if (byteLength === 4)
    return this.readInt32BE(offset);
  if (byteLength === 2)
    return this.readInt16BE(offset);
  if (byteLength === 1)
    return this.readInt8(offset);

  boundsError(byteLength, 6, 'byteLength');
}

function readInt48BE(buf, offset = 0) {
  checkBounds(buf, offset, 6);

  const val = buf[kIBDV].getUint16(buf.byteOffset + offset);
  return (val | (val & 2 ** 15) * 0x1fffe) * 2 ** 32 +
    buf[kIBDV].getUint32(buf.byteOffset + offset + 2);
}

function readInt40BE(buf, offset = 0) {
  checkBounds(buf, offset, 5);

  const first = buf[offset];
  return (first | (first & 2 ** 7) * 0x1fffffe) * 2 ** 32 +
    buf[kIBDV].getUint32(buf.byteOffset + offset + 1);
}

function readInt32BE(offset = 0) {
  checkBounds(this, offset, 4);

  return this[kIBDV].getInt32(this.byteOffset + offset);
}

function readInt24BE(buf, offset = 0) {
  checkBounds(buf, offset, 3);

  const val = buf[offset] * 2 ** 16 +
    buf[kIBDV].getUint16(buf.byteOffset + offset + 1);
  return val | (val & 2 ** 23) * 0x1fe;
}

function readInt16BE(offset = 0) {
  checkBounds(this, offset, 2);

  return this[kIBDV].getInt16(this.byteOffset + offset);
}

// Read floats
function readFloatBE(offset = 0) {
  checkBounds(this, offset, 4);

  return this[kIBDV].getFloat32(this.byteOffset + offset);
}

function readFloatLE(offset = 0) {
  checkBounds(this, offset, 4);

  return this[kIBDV].getFloat32(this.byteOffset + offset, true);
}

function readDoubleBE(offset = 0) {
  checkBounds(this, offset, 8);

  return this[kIBDV].getFloat64(this.byteOffset + offset);
}

function readDoubleLE(offset = 0) {
  checkBounds(this, offset, 8);

  return this[kIBDV].getFloat64(this.byteOffset + offset, true);
}

// Write integers.
function writeUIntLE(value, offset, byteLength) {
  if (byteLength === 6)
    return writeU_Int48LE(this, value, offset, 0, 0xffffffffffff);
  if (byteLength === 5)
    return writeU_Int40LE(this, value, offset, 0, 0xffffffffff);
  if (byteLength === 3)
    return writeU_Int24LE(this, value, offset, 0, 0xffffff);
  if (byteLength === 4)
    return writeU_Int32LE(this, value, offset, 0, 0xffffffff);
  if (byteLength === 2)
    return writeU_Int16LE(this, value, offset, 0, 0xffff);
  if (byteLength === 1)
    return writeU_Int8(this, value, offset, 0, 0xff);

  boundsError(byteLength, 6, 'byteLength');
}

function writeU_Int48LE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 6);

  buf[kIBDV].setUint32(buf.byteOffset + offset, value, true);
  buf[kIBDV].setUint16(buf.byteOffset + offset + 4, value * 2 ** -32, true);
  return offset + 6;
}

function writeU_Int40LE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 5);

  buf[kIBDV].setUint32(buf.byteOffset + offset, value, true);
  buf[kIBDV].setUint8(buf.byteOffset + offset + 4, value * 2 ** -32);
  return offset + 5;
}

function writeU_Int32LE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 4);

  buf[kIBDV].setUint32(buf.byteOffset + offset, value, true);
  return offset + 4;
}

function writeUInt32LE(value, offset = 0) {
  return writeU_Int32LE(this, value, offset, 0, 0xffffffff);
}

function writeU_Int24LE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 3);

  buf[kIBDV].setUint16(buf.byteOffset + offset, value, true);
  buf[kIBDV].setUint8(buf.byteOffset + offset + 2, value >>> 16);
  return offset + 3;
}

function writeU_Int16LE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 2);

  buf[kIBDV].setUint16(buf.byteOffset + offset, value, true);
  return offset + 2;
}

function writeUInt16LE(value, offset = 0) {
  return writeU_Int16LE(this, value, offset, 0, 0xffff);
}

function writeU_Int8(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 1);

  buf[offset] = value;
  return offset + 1;
}

function writeUInt8(value, offset = 0) {
  return writeU_Int8(this, value, offset, 0, 0xff);
}

function writeUIntBE(value, offset, byteLength) {
  if (byteLength === 6)
    return writeU_Int48BE(this, value, offset, 0, 0xffffffffffffff);
  if (byteLength === 5)
    return writeU_Int40BE(this, value, offset, 0, 0xffffffffff);
  if (byteLength === 3)
    return writeU_Int24BE(this, value, offset, 0, 0xffffff);
  if (byteLength === 4)
    return writeU_Int32BE(this, value, offset, 0, 0xffffffff);
  if (byteLength === 2)
    return writeU_Int16BE(this, value, offset, 0, 0xffff);
  if (byteLength === 1)
    return writeU_Int8(this, value, offset, 0, 0xff);

  boundsError(byteLength, 6, 'byteLength');
}

function writeU_Int48BE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 6);

  buf[kIBDV].setUint16(buf.byteOffset + offset, value * 2 ** -32);
  buf[kIBDV].setUint32(buf.byteOffset + offset + 2, value);
  return offset + 6;
}

function writeU_Int40BE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 5);

  buf[kIBDV].setUint8(buf.byteOffset + offset, value * 2 ** -32);
  buf[kIBDV].setUint32(buf.byteOffset + offset + 1, value);
  return offset + 5;
}

function writeU_Int32BE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 4);

  buf[kIBDV].setUint32(buf.byteOffset + offset, value);
  return offset + 4;
}

function writeUInt32BE(value, offset = 0) {
  return writeU_Int32BE(this, value, offset, 0, 0xffffffff);
}

function writeU_Int24BE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 3);

  buf[kIBDV].setUint16(buf.byteOffset + offset, value >>> 8);
  buf[offset + 2] = value;
  return offset + 3;
}

function writeU_Int16BE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 2);

  buf[kIBDV].setUint16(buf.byteOffset + offset, value);
  return offset + 2;
}

function writeUInt16BE(value, offset = 0) {
  return writeU_Int16BE(this, value, offset, 0, 0xffff);
}

function writeIntLE(value, offset, byteLength) {
  if (byteLength === 6)
    return writeU_Int48LE(this, value, offset, -0x800000000000, 0x7fffffffffff);
  if (byteLength === 5)
    return writeU_Int40LE(this, value, offset, -0x8000000000, 0x7fffffffff);
  if (byteLength === 3)
    return writeU_Int24LE(this, value, offset, -0x800000, 0x7fffff);
  if (byteLength === 4)
    return writeU_Int32LE(this, value, offset, -0x80000000, 0x7fffffff);
  if (byteLength === 2)
    return writeU_Int16LE(this, value, offset, -0x8000, 0x7fff);
  if (byteLength === 1)
    return writeU_Int8(this, value, offset, -0x80, 0x7f);

  boundsError(byteLength, 6, 'byteLength');
}

function writeInt32LE(value, offset = 0) {
  return writeU_Int32LE(this, value, offset, -0x80000000, 0x7fffffff);
}

function writeInt16LE(value, offset = 0) {
  return writeU_Int16LE(this, value, offset, -0x8000, 0x7fff);
}

function writeInt8(value, offset = 0) {
  return writeU_Int8(this, value, offset, -0x80, 0x7f);
}

function writeIntBE(value, offset, byteLength) {
  if (byteLength === 6)
    return writeU_Int48BE(this, value, offset, -0x800000000000, 0x7fffffffffff);
  if (byteLength === 5)
    return writeU_Int40BE(this, value, offset, -0x8000000000, 0x7fffffffff);
  if (byteLength === 3)
    return writeU_Int24BE(this, value, offset, -0x800000, 0x7fffff);
  if (byteLength === 4)
    return writeU_Int32BE(this, value, offset, -0x80000000, 0x7fffffff);
  if (byteLength === 2)
    return writeU_Int16BE(this, value, offset, -0x8000, 0x7fff);
  if (byteLength === 1)
    return writeU_Int8(this, value, offset, -0x80, 0x7f);

  boundsError(byteLength, 6, 'byteLength');
}

function writeInt32BE(value, offset = 0) {
  return writeU_Int32BE(this, value, offset, -0x80000000, 0x7fffffff);
}

function writeInt16BE(value, offset = 0) {
  return writeU_Int16BE(this, value, offset, -0x8000, 0x7fff);
}

// Write doubles.
function writeDoubleBE(val, offset = 0) {
  val = +val;
  checkBounds(this, offset, 8);

  this[kIBDV].setFloat64(this.byteOffset + offset, val);
  return offset + 8;
}

function writeDoubleLE(val, offset = 0) {
  val = +val;
  checkBounds(this, offset, 8);

  this[kIBDV].setFloat64(this.byteOffset + offset, val, true);
  return offset + 8;
}

function writeFloatBE(val, offset = 0) {
  val = +val;
  checkBounds(this, offset, 4);

  this[kIBDV].setFloat32(this.byteOffset + offset, val);
  return offset + 4;
}

function writeFloatLE(val, offset = 0) {
  val = +val;
  checkBounds(this, offset, 4);

  this[kIBDV].setFloat32(this.byteOffset + offset, val, true);
  return offset + 4;
}

// FastBuffer wil be inserted here by lib/buffer.js
module.exports = {
  setupBufferJS,
  // Container to export all read write functions.
  readWrites: {
    readUIntLE,
    readUInt32LE,
    readUInt16LE,
    readUInt8,
    readUIntBE,
    readUInt32BE,
    readUInt16BE,
    readIntLE,
    readInt32LE,
    readInt16LE,
    readInt8,
    readIntBE,
    readInt32BE,
    readInt16BE,
    writeUIntLE,
    writeUInt32LE,
    writeUInt16LE,
    writeUInt8,
    writeUIntBE,
    writeUInt32BE,
    writeUInt16BE,
    writeIntLE,
    writeInt32LE,
    writeInt16LE,
    writeInt8,
    writeIntBE,
    writeInt32BE,
    writeInt16BE,
    readFloatLE,
    readFloatBE,
    readDoubleLE,
    readDoubleBE,
    writeFloatLE,
    writeFloatBE,
    writeDoubleLE,
    writeDoubleBE
  },
  // function and symbol for internal buffer data view usage
  IBDV: { getIBDV, kIBDV }
};
