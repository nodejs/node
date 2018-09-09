'use strict';

const binding = process.binding('buffer');
const {
  ERR_BUFFER_OUT_OF_BOUNDS,
  ERR_INVALID_ARG_TYPE,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const { validateNumber } = require('internal/validators');
const { setupBufferJS } = binding;

// Remove from the binding so that function is only available as exported here.
// (That is, for internal use only.)
delete binding.setupBufferJS;

// DataView WeakMap-based mappings for read/write functions.
const dvwm = new WeakMap();
const gdv = (arrbuf) => dvwm.get(arrbuf) ||
  dvwm.set(arrbuf, new DataView(arrbuf)).get(arrbuf);

function checkBounds(buf, offset, byteLength) {
  checkNumberType(offset);
  if (buf[offset] === undefined || buf[offset + byteLength] === undefined)
    boundsError(offset, buf.byteLength - (byteLength + 1));
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
function readBigUInt128LE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 16)
    boundsError(offset, this.byteLength - 16);

  return this._dataview.getBigUint64(this.byteOffset + offset + 8, true) +
    (this._dataview.getBigUint64(this.byteOffset + offset, true) << 64n);
}

function readBigUInt64LE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 8)
    boundsError(offset, this.byteLength - 8);

  return this._dataview.getBigUint64(this.byteOffset + offset, true);
}

function readUIntLE(offset, byteLength) {
  if (offset === undefined)
    throw new ERR_INVALID_ARG_TYPE('offset', 'number', offset);
  switch (byteLength) {
    case 6:
      return readUInt48LE(this, offset);

    case 5:
      return readUInt40LE(this, offset);

    case 4:
      return this.readUInt32LE(offset);

    case 3:
      return readUInt24LE(this, offset);

    case 2:
      return this.readUInt16LE(offset);

    case 1:
      return this.readUInt8(offset);
  }

  boundsError(byteLength, 6, 'byteLength');
}

function readUInt48LE(buf, offset = 0) {
  checkNumberType(offset);
  if (buf.byteLength < offset + 6)
    boundsError(offset, buf.byteLength - 6);

  return buf._dataview.getUint16(buf.byteOffset + offset, true) +
    buf._dataview.getUint32(buf.byteOffset + offset + 2, true) * 2 ** 16;
}

function readUInt40LE(buf, offset = 0) {
  checkNumberType(offset);
  if (buf.byteLength < offset + 5)
    boundsError(offset, buf.byteLength - 5);

  return buf._dataview.getUint8(buf.byteOffset + offset) +
    buf._dataview.getUint32(buf.byteOffset + offset + 1, true) * 2 ** 8;
}

function readUInt32LE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 4)
    boundsError(offset, this.byteLength - 4);

  return this._dataview.getUint32(this.byteOffset + offset, true);
}

function readUInt24LE(buf, offset = 0) {
  checkNumberType(offset);
  if (buf.byteLength < offset + 3)
    boundsError(offset, buf.byteLength - 3);

  return buf._dataview.getUint8(buf.byteOffset + offset) +
    buf._dataview.getUint16(buf.byteOffset + offset + 1, true) * 2 ** 8;
}

function readUInt16LE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 2)
    boundsError(offset, this.byteLength - 2);

  return this._dataview.getUint16(this.byteOffset + offset, true);
}

function readUInt8(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 1)
    boundsError(offset, this.byteLength - 1);

  // meh
  return this[offset];
}

function readBigUInt128BE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 16)
    boundsError(offset, this.byteLength - 16);

  return this._dataview.getBigUint64(this.byteOffset + offset) +
    (this._dataview.getBigUint64(this.byteOffset + offset + 8) << 64n);
}

function readBigUInt64BE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 8)
    boundsError(offset, this.byteLength - 8);

  return this._dataview.getBigUint64(this.byteOffset + offset);
}

function readUIntBE(offset, byteLength) {
  if (offset === undefined)
    throw new ERR_INVALID_ARG_TYPE('offset', 'number', offset);
  switch (byteLength) {
    case 6:
      return readUInt48BE(this, offset);

    case 5:
      return readUInt40BE(this, offset);

    case 4:
      return this.readUInt32BE(offset);

    case 3:
      return readUInt24BE(this, offset);

    case 2:
      return this.readUInt16BE(offset);

    case 1:
      return this.readUInt8(offset);
  }

  boundsError(byteLength, 6, 'byteLength');
}

function readUInt48BE(buf, offset = 0) {
  checkNumberType(offset);
  if (buf.byteLength < offset + 6)
    boundsError(offset, buf.byteLength - 6);

  return buf._dataview.getUint32(buf.byteOffset + offset + 2) +
    buf._dataview.getUint16(buf.byteOffset + offset) * 2 ** 32;
}

function readUInt40BE(buf, offset = 0) {
  checkNumberType(offset);
  if (buf.byteLength < offset + 5)
    boundsError(offset, buf.byteLength - 5);

  return buf._dataview.getUint32(buf.byteOffset + offset + 1) +
    buf._dataview.getUint8(buf.byteOffset + offset) * 2 ** 32;
}

function readUInt32BE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 4)
    boundsError(offset, this.byteLength - 4);

  return this._dataview.getUint32(this.byteOffset + offset);
}

function readUInt24BE(buf, offset = 0) {
  checkNumberType(offset);
  if (buf.byteLength < offset + 3)
    boundsError(offset, buf.byteLength - 3);

  return buf._dataview.getUint16(buf.byteOffset + offset + 1) +
    buf._dataview.getUint8(buf.byteOffset + offset) * 2 ** 16;
}

function readUInt16BE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 2)
    boundsError(offset, this.byteLength - 2);

  return this._dataview.getUint16(this.byteOffset + offset);
}

function readBigInt128LE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 16)
    boundsError(offset, this.byteLength - 16);

  return this._dataview.getBigUint64(this.byteOffset + offset + 8, true) |
    this._dataview.getBigInt64(this.byteOffset + offset, true) << 64n
}

function readBigInt64LE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 8)
    boundsError(offset, this.byteLength - 8);

  return this._dataview.getBigInt64(this.byteOffset + offset, true);
}
function readIntLE(offset, byteLength) {
  if (offset === undefined)
    throw new ERR_INVALID_ARG_TYPE('offset', 'number', offset);
  switch (byteLength) {
    case 6:
      return readInt48LE(this, offset);

    case 5:
      return readInt40LE(this, offset);

    case 4:
      return this.readInt32LE(offset);

    case 3:
      return readInt24LE(this, offset);

    case 2:
      return this.readInt16LE(offset);

    case 1:
      return this.readInt8(offset);
  }

  boundsError(byteLength, 6, 'byteLength');
}

function readInt48LE(buf, offset = 0) {
  checkNumberType(offset);
  if (buf.byteLength < offset + 6)
    boundsError(offset, buf.byteLength - 6);

  const val = buf._dataview.getUint16(buf.byteOffset + offset + 4, true);
  return (val | (val & 2 ** 15) * 0x1fffe) * 2 ** 32 +
    buf._dataview.getUint32(buf.byteOffset + offset, true);
}

function readInt40LE(buf, offset = 0) {
  checkNumberType(offset);
  if (buf.byteLength < offset + 5)
    boundsError(offset, buf.byteLength - 5);

  const last = buf._dataview.getUint8(buf.byteOffset + offset + 4);
  return (last | (last & 2 ** 7) * 0x1fffffe) * 2 ** 32 +
    buf._dataview.getUint32(buf.byteOffset + offset, true);
}

function readInt32LE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 4)
    boundsError(offset, this.byteLength - 4);

  return this._dataview.getInt32(this.byteOffset + offset);
}

function readInt24LE(buf, offset = 0) {
  checkNumberType(offset);
  if (buf.byteLength < offset + 3)
    boundsError(offset, buf.byteLength - 3);

  return buf._dataview.getUint8(buf.byteOffset + offset + 2) |
    buf._dataview.getInt16(buf.byteOffset + offset, true) << 8;
}

function readInt16LE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 2)
    boundsError(offset, this.byteLength - 2);

  return this._dataview.getInt16(this.byteOffset + offset, true);
}

function readInt8(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset)
    boundsError(offset, this.byteLength - 1);

  return this._dataview.getInt8(this.byteOffset + offset);
}

function readBigInt128BE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 16)
    boundsError(offset, this.byteLength - 16);

  return this._dataview.getBigUint64(this.byteOffset + offset) |
    this._dataview.getBigInt64(this.byteOffset + offset + 8) << 64n;
}

function readBigInt64BE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 8)
    boundsError(offset, this.byteLength - 8);

  return this._dataview.getBigInt64(this.byteOffset + offset);
}

function readIntBE(offset, byteLength) {
  if (offset === undefined)
    throw new ERR_INVALID_ARG_TYPE('offset', 'number', offset);
  switch (byteLength) {
    case 6:
      return readInt48BE(this, offset);

    case 5:
      return readInt40BE(this, offset);

    case 4:
      return this.readInt32BE(offset);

    case 3:
      return readInt24BE(this, offset);

    case 2:
      return this.readInt16BE(offset);

    case 1:
      return this.readInt8(offset);
  }

  boundsError(byteLength, 6, 'byteLength');
}

function readInt48BE(buf, offset = 0) {
  checkNumberType(offset);
  if (buf.byteLength < offset + 6)
    boundsError(offset, buf.byteLength - 6);

  const val = buf._dataview.getUint16(buf.byteOffset + offset);
  return (val | (val & 2 ** 15) * 0x1fffe) * 2 ** 32 +
    buf._dataview.getUint32(buf.byteOffset + offset + 2);
}

function readInt40BE(buf, offset = 0) {
  checkNumberType(offset);
  if (buf.byteLength < offset + 5)
    boundsError(offset, buf.byteLength - 5);

  const first = buf._dataview.getUint8(buf.byteOffset + offset);
  return (first | (first & 2 ** 7) * 0x1fffffe) * 2 ** 32 +
    buf._dataview.getUint32(buf.byteOffset + offset + 1);
}

function readInt32BE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 4)
    boundsError(offset, this.byteLength - 4);

  return this._dataview.getInt32(this.byteOffset + offset);
}

function readInt24BE(buf, offset = 0) {
  checkNumberType(offset);
  const first = buf[offset];
  const last = buf[offset + 2];
  if (first === undefined || last === undefined)
    boundsError(offset, buf.byteLength - 3);

  const val = first * 2 ** 16 + buf[++offset] * 2 ** 8 + last;
  return val | (val & 2 ** 23) * 0x1fe;
}

function readInt16BE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 2)
    boundsError(offset, this.byteLength - 2);

  return this._dataview.getInt16(this.byteOffset + offset);
}

// Read floats
function readFloatLE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 4)
    boundsError(offset, this.byteLength - 4);

  return this._dataview.getFloat32(this.byteOffset + offset, true);
}

function readFloatBE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 4)
    boundsError(offset, this.byteLength - 4);

  return this._dataview.getFloat32(this.byteOffset + offset);
}

// Read doubles
function readDoubleLE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 8)
    boundsError(offset, this.byteLength - 8);

  return this._dataview.getFloat64(this.byteOffset + offset, true);
}

function readDoubleBE(offset = 0) {
  checkNumberType(offset);
  if (this.byteLength < offset + 8)
    boundsError(offset, this.byteLength - 8);

  return this._dataview.getFloat64(this.byteOffset + offset);
}

// Write integers
function writeBigU_Int128LE(buf, value, offset, min, max) {
  value = BigInt(value);
  checkInt(value, min, max, buf, offset, 15);

  buf._dataview.setBigUint64(buf.byteOffset + offset, value, true);
  buf._dataview.setBigUint64(buf.byteOffset + offset + 8, value >> 64n, true);
  return offset + 16;
}

const BIG_UINT_128_MAX = BigInt.asUintN(128, -1n);

function writeBigUInt128LE(value, offset = 0) {
  return writeBigU_Int128LE(this, value, offset, 0n, BIG_UINT_128_MAX);
}

function writeBigU_Int64LE(buf, value, offset, min, max) {
  value = BigInt(value);
  checkInt(value, min, max, buf, offset, 7);

  buf._dataview.setBigUint64(buf.byteOffset + offset, value, true);
  return offset + 8;
}

const BIG_UINT_64_MAX = BigInt.asUintN(64, -1n);

function writeBigUInt64LE(value, offset = 0) {
  return writeBigU_Int64LE(this, value, offset, 0n, BIG_UINT_64_MAX);
}

function writeUIntLE(value, offset, byteLength) {
  switch (byteLength) {
    case 6:
      return writeU_Int48LE(this, value, offset,
                            0, 0xffffffffffff);

    case 5:
      return writeU_Int40LE(this, value, offset,
                            0, 0xffffffffff);

    case 4:
      return writeU_Int32LE(this, value, offset,
                            0, 0xffffffff);

    case 3:
      return writeU_Int24LE(this, value, offset,
                            0, 0xffffff);

    case 2:
      return writeU_Int16LE(this, value, offset,
                            0, 0xffff);

    case 1:
      return writeU_Int8(this, value, offset, 0, 0xff);
  }

  boundsError(byteLength, 6, 'byteLength');
}

function writeU_Int48LE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 5);

  const newVal = Math.floor(value * 2 ** -32);

  buf._dataview.setUint32(buf.byteOffset + offset, value, true);
  buf._dataview.setUint16(buf.byteOffset + offset + 4, newVal, true);
  return offset + 6;
}

function writeU_Int40LE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 4);

  const newVal = Math.floor(value * 2 ** -32);

  buf._dataview.setUint32(buf.byteOffset + offset, value, true);
  buf._dataview.setUint8(buf.byteOffset + offset + 4, newVal);
  return offset + 5;
}

function writeU_Int32LE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 3);

  gdv(buf.buffer).setUint32(buf.byteOffset + offset, value, true);
  return offset + 4;
}

function writeUInt32LE(value, offset = 0) {
  return writeU_Int32LE(this, value, offset, 0, 0xffffffff);
}

function writeU_Int24LE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 2);

  buf._dataview.setUint16(buf.byteOffset + offset, value, true);
  buf._dataview.setUint8(buf.byteOffset + offset, value >>> 16, true);
  return offset + 3;
}

function writeU_Int16LE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 1);

  buf._dataview.setUint16(buf.byteOffset + offset, value, true);
  return offset + 2;
}

function writeUInt16LE(value, offset = 0) {
  return writeU_Int16LE(this, value, offset, 0, 0xffff);
}

function writeU_Int8(buf, value, offset, min, max) {
  value = +value;
  // `checkInt()` can not be used here because it checks two entries.
  checkNumberType(offset);
  if (value > max || value < min) {
    throw new ERR_OUT_OF_RANGE('value', `>= ${min} and <= ${max}`, value);
  }
  if (buf[offset] === undefined)
    boundsError(offset, buf.byteLength - 1);

  buf[offset] = value;
  return offset + 1;
}

function writeUInt8(value, offset = 0) {
  return writeU_Int8(this, value, offset, 0, 0xff);
}

function writeBigU_Int128BE(buf, value, offset, min, max) {
  value = BigInt(value);
  checkInt(value, min, max, buf, offset, 15);

  const newVal = value >> 64n;

  buf._dataview.setBigUint64(buf.byteOffset + offset, newVal);
  buf._dataview.setBigUint64(buf.byteOffset + offset + 8, value);
  return offset + 16;
}

function writeBigUInt128BE(value, offset = 0) {
  return writeBigU_Int128BE(this, value, offset, 0n, BIG_UINT_128_MAX);
}

function writeBigU_Int64BE(buf, value, offset, min, max) {
  value = BigInt(value);
  checkInt(value, min, max, buf, offset, 7);

  buf._dataview.setBigUint64(buf.byteOffset + offset, value);
  return offset + 8;
}

function writeBigUInt64BE(value, offset = 0) {
  return writeBigU_Int64BE(this, value, offset, 0n, BIG_UINT_64_MAX);
}

function writeUIntBE(value, offset, byteLength) {
  switch (byteLength) {
    case 6:
      return writeU_Int48BE(this, value, offset, 0, 0xffffffffffffff);

    case 5:
      return writeU_Int40BE(this, value, offset, 0, 0xffffffffff);

    case 4:
      return writeU_Int32BE(this, value, offset, 0, 0xffffffff);

    case 3:
      return writeU_Int24BE(this, value, offset, 0, 0xffffff);

    case 2:
      return writeU_Int16BE(this, value, offset, 0, 0xffff);

    case 1:
      return writeU_Int8(this, value, offset, 0, 0xff);
  }

  boundsError(byteLength, 6, 'byteLength');
}

function writeU_Int48BE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 5);

  const newVal = Math.floor(value * 2 ** -32);

  buf._dataview.setUint16(buf.byteOffset + offset, newVal);
  buf._dataview.setUint32(buf.byteOffset + offset + 2, value);
  return offset + 6;
}

function writeU_Int40BE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 4);

  buf[offset++] = Math.floor(value * 2 ** -32);
  buf._dataview.setUint32(buf.byteOffset + offset + 1, value);
  return offset + 5;
}

function writeU_Int32BE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 3);

  buf._dataview.setUint32(buf.byteOffset + offset, value);
  return offset + 4;
}

function writeUInt32BE(value, offset = 0) {
  return writeU_Int32BE(this, value, offset, 0, 0xffffffff);
}

function writeU_Int24BE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 2);

  buf._dataview.setUint16(buf.byteOffset + offset + 1, value);
  buf._dataview.setUint8(buf.byteOffset + offset, value >>> 16);
  return offset + 3;
}

function writeU_Int16BE(buf, value, offset, min, max) {
  value = +value;
  checkInt(value, min, max, buf, offset, 1);

  buf._dataview.setUint16(buf.byteOffset + offset, value);
  return offset + 2;
}

function writeUInt16BE(value, offset = 0) {
  return writeU_Int16BE(this, value, offset, 0, 0xffffffff);
}

// Do it with bits to not type out 31 zeros.
const BIG_INT_128_MIN = -(1n << 127n);
const BIG_INT_128_MAX = BigInt.asUintN(127, -1n);

function writeBigInt128LE(value, offset = 0) {
  return writeBigU_Int128LE(this, value, offset,
                            BIG_INT_128_MIN, BIG_INT_128_MAX);
}

// We don't want to type out 15 zeros.
const BIG_INT_64_MIN = -(1n << 63n);
const BIG_INT_64_MAX = BigInt.asUintN(63, -1n);

function writeBigInt64LE(value, offset = 0) {
  return writeBigU_Int64LE(this, value, offset,
                           BIG_INT_64_MIN, BIG_INT_64_MAX);
}

function writeIntLE(value, offset, byteLength) {
  switch (byteLength) {
    case 6:
      return writeU_Int48LE(this, value, offset,
                            -0x800000000000, 0x7fffffffffff);

    case 5:
      return writeU_Int40LE(this, value, offset,
                            -0x8000000000, 0x7fffffffff);

    case 4:
      return writeU_Int32LE(this, value, offset,
                            -0x80000000, 0x7fffffff);
    case 3:
      return writeU_Int24LE(this, value, offset,
                            -0x800000, 0x7fffff);
    case 2:
      return writeU_Int16LE(this, value, offset,
                            -0x8000, 0x7fff);
    case 1:
      return writeU_Int8(this, value, offset, -0x80, 0x7f);
  }

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

function writeBigInt128BE(value, offset = 0) {
  return writeBigU_Int128BE(this, value, offset,
                            BIG_INT_128_MIN, BIG_INT_128_MAX);
}

function writeBigInt64BE(value, offset = 0) {
  return writeBigU_Int64BE(this, value, offset,
                           BIG_INT_64_MIN, BIG_INT_64_MAX);
}

function writeIntBE(value, offset, byteLength) {
  switch (byteLength) {
    case 6:
      return writeU_Int48BE(this, value, offset,
                            -0x800000000000, 0x7fffffffffff);
    case 5:
      return writeU_Int40BE(this, value, offset,
                            -0x8000000000, 0x7fffffffff);
    case 4:
      return writeU_Int32BE(this, value, offset,
                            -0x80000000, 0x7fffffff);

    case 3:
      return writeU_Int24BE(this, value, offset,
                            -0x800000, 0x7fffff);

    case 2:
      return writeU_Int16BE(this, value, offset,
                            -0x8000, 0x7fff);

    case 1:
      return writeU_Int8(this, value, offset, -0x80, 0x7f);
  }

  boundsError(byteLength, 6, 'byteLength');
}

function writeInt32BE(value, offset = 0) {
  return writeU_Int32BE(this, value, offset, -0x80000000, 0x7fffffff);
}

function writeInt16BE(value, offset = 0) {
  return writeU_Int16BE(this, value, offset, -0x8000, 0x7fff);
}

// Write doubles.
function writeDoubleLE(val, offset = 0) {
  val = +val;
  checkBounds(this, offset, 7);

  this._dataview.setFloat64(this.byteOffset + offset, val, true);
  return offset + 8;
}

function writeDoubleBE(val, offset = 0) {
  val = +val;
  checkBounds(this, offset, 7);

  this._dataview.setFloat64(this.byteOffset + offset, val);
  return offset + 8;
}

// Write floats.
function writeFloatLE(val, offset = 0) {
  val = +val;
  checkBounds(this, offset, 3);

  this._dataview.setFloat32(this.byteOffset + offset, val, true);
  return offset + 4;
}

function writeFloatBE(val, offset = 0) {
  val = +val;
  checkBounds(this, offset, 3);

  this._dataview.setFloat32(this.byteOffset + offset, val);
  return offset + 4;
}

// FastBuffer wil be inserted here by lib/buffer.js
module.exports = {
  setupBufferJS,
  // Container to export all read write functions.
  readWrites: {
    // Read unsigned integers
    readBigUInt128LE,
    readBigUInt64LE,
    readUIntLE,
    readUInt32LE,
    readUInt16LE,
    readUInt8,
    readBigUInt128BE,
    readBigUInt64BE,
    readUIntBE,
    readUInt32BE,
    readUInt16BE,
    // Read signed integers
    readBigInt128LE,
    readBigInt64LE,
    readIntLE,
    readInt32LE,
    readInt16LE,
    readBigInt128BE,
    readBigInt64BE,
    readInt8,
    readIntBE,
    readInt32BE,
    readInt16BE,
    // Write unsigned integers
    writeBigUInt128LE,
    writeBigUInt64LE,
    writeUIntLE,
    writeUInt32LE,
    writeUInt16LE,
    writeUInt8,
    writeBigUInt128BE,
    writeBigUInt64BE,
    writeUIntBE,
    writeUInt32BE,
    writeUInt16BE,
    // Write signed integers
    writeBigInt128LE,
    writeBigInt64LE,
    writeIntLE,
    writeInt32LE,
    writeInt16LE,
    writeInt8,
    writeBigInt128BE,
    writeBigInt64BE,
    writeIntBE,
    writeInt32BE,
    writeInt16BE,
    // Read floats
    readFloatLE,
    readFloatBE,
    readDoubleLE,
    readDoubleBE,
    // Write floats
    writeFloatLE,
    writeFloatBE,
    writeDoubleLE,
    writeDoubleBE
  },
  dataView: {
    dvwm,
    gdv
  }
};
