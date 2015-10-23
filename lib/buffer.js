/* eslint-disable require-buffer */
'use strict';

const binding = process.binding('buffer');
const internalUtil = require('internal/util');
const bindingObj = {};

exports.Buffer = Buffer;
exports.SlowBuffer = SlowBuffer;
exports.INSPECT_MAX_BYTES = 50;
exports.kMaxLength = binding.kMaxLength;


Buffer.poolSize = 8 * 1024;
var poolSize, poolOffset, allocPool;


binding.setupBufferJS(Buffer.prototype, bindingObj);
const flags = bindingObj.flags;
const kNoZeroFill = 0;


function createPool() {
  poolSize = Buffer.poolSize;
  if (poolSize > 0)
    flags[kNoZeroFill] = 1;
  allocPool = new Uint8Array(poolSize);
  Object.setPrototypeOf(allocPool, Buffer.prototype);
  poolOffset = 0;
}
createPool();


function alignPool() {
  // Ensure aligned slices
  if (poolOffset & 0x7) {
    poolOffset |= 0x7;
    poolOffset++;
  }
}


function Buffer(arg) {
  // Common case.
  if (typeof arg === 'number') {
    // If less than zero, or NaN.
    if (arg < 0 || arg !== arg)
      arg = 0;
    return allocate(arg);
  }

  // Slightly less common case.
  if (typeof arg === 'string') {
    return fromString(arg, arguments[1]);
  }

  // Unusual.
  return fromObject(arg);
}

Object.setPrototypeOf(Buffer.prototype, Uint8Array.prototype);
Object.setPrototypeOf(Buffer, Uint8Array);


function SlowBuffer(length) {
  if (+length != length)
    length = 0;
  if (length > 0)
    flags[kNoZeroFill] = 1;
  const ui8 = new Uint8Array(+length);
  Object.setPrototypeOf(ui8, Buffer.prototype);
  return ui8;
}

Object.setPrototypeOf(SlowBuffer.prototype, Uint8Array.prototype);
Object.setPrototypeOf(SlowBuffer, Uint8Array);


function allocate(size) {
  if (size === 0) {
    const ui8 = new Uint8Array(size);
    Object.setPrototypeOf(ui8, Buffer.prototype);
    return ui8;
  }
  if (size < (Buffer.poolSize >>> 1)) {
    if (size > (poolSize - poolOffset))
      createPool();
    var b = allocPool.slice(poolOffset, poolOffset + size);
    poolOffset += size;
    alignPool();
    return b;
  } else {
    // Even though this is checked above, the conditional is a safety net and
    // sanity check to prevent any subsequent typed array allocation from not
    // being zero filled.
    if (size > 0)
      flags[kNoZeroFill] = 1;
    const ui8 = new Uint8Array(size);
    Object.setPrototypeOf(ui8, Buffer.prototype);
    return ui8;
  }
}


function fromString(string, encoding) {
  if (typeof encoding !== 'string' || encoding === '')
    encoding = 'utf8';

  var length = byteLength(string, encoding);
  if (length >= (Buffer.poolSize >>> 1))
    return binding.createFromString(string, encoding);

  if (length > (poolSize - poolOffset))
    createPool();
  var actual = allocPool.write(string, poolOffset, encoding);
  var b = allocPool.slice(poolOffset, poolOffset + actual);
  poolOffset += actual;
  alignPool();
  return b;
}


function fromObject(obj) {
  if (obj instanceof Buffer) {
    var b = allocate(obj.length);
    obj.copy(b, 0, 0, obj.length);
    return b;
  }

  if (Array.isArray(obj)) {
    var length = obj.length;
    var b = allocate(length);
    for (var i = 0; i < length; i++)
      b[i] = obj[i] & 255;
    return b;
  }

  if (obj == null) {
    throw new TypeError('must start with number, buffer, array or string');
  }

  if (obj instanceof ArrayBuffer) {
    return binding.createFromArrayBuffer(obj);
  }

  if (obj.buffer instanceof ArrayBuffer || obj.length) {
    var length;
    if (typeof obj.length !== 'number' || obj.length !== obj.length)
      length = 0;
    else
      length = obj.length;
    var b = allocate(length);
    for (var i = 0; i < length; i++) {
      b[i] = obj[i] & 255;
    }
    return b;
  }

  if (obj.type === 'Buffer' && Array.isArray(obj.data)) {
    var array = obj.data;
    var b = allocate(array.length);
    for (var i = 0; i < array.length; i++)
      b[i] = array[i] & 255;
    return b;
  }

  throw new TypeError('must start with number, buffer, array or string');
}


// Static methods

Buffer.isBuffer = function isBuffer(b) {
  return b instanceof Buffer;
};


Buffer.compare = function compare(a, b) {
  if (!(a instanceof Buffer) ||
      !(b instanceof Buffer)) {
    throw new TypeError('Arguments must be Buffers');
  }

  if (a === b) {
    return 0;
  }

  return binding.compare(a, b);
};


Buffer.isEncoding = function(encoding) {
  var loweredCase = false;
  for (;;) {
    switch (encoding) {
      case 'hex':
      case 'utf8':
      case 'utf-8':
      case 'ascii':
      case 'binary':
      case 'base64':
      case 'ucs2':
      case 'ucs-2':
      case 'utf16le':
      case 'utf-16le':
        return true;

      default:
        if (loweredCase)
          return false;
        encoding = ('' + encoding).toLowerCase();
        loweredCase = true;
    }
  }
};


Buffer.concat = function(list, length) {
  if (!Array.isArray(list))
    throw new TypeError('list argument must be an Array of Buffers.');

  if (list.length === 0)
    return new Buffer(0);

  if (length === undefined) {
    length = 0;
    for (var i = 0; i < list.length; i++)
      length += list[i].length;
  } else {
    length = length >>> 0;
  }

  var buffer = new Buffer(length);
  var pos = 0;
  for (var i = 0; i < list.length; i++) {
    var buf = list[i];
    buf.copy(buffer, pos);
    pos += buf.length;
  }

  return buffer;
};


function base64ByteLength(str, bytes) {
  // Handle padding
  if (str.charCodeAt(bytes - 1) === 0x3D)
    bytes--;
  if (bytes > 1 && str.charCodeAt(bytes - 1) === 0x3D)
    bytes--;

  // Base64 ratio: 3/4
  return (bytes * 3) >>> 2;
}


function byteLength(string, encoding) {
  if (typeof string !== 'string')
    string = '' + string;

  var len = string.length;
  if (len === 0)
    return 0;

  // Use a for loop to avoid recursion
  var loweredCase = false;
  for (;;) {
    switch (encoding) {
      case 'ascii':
      case 'binary':
        return len;

      case 'utf8':
      case 'utf-8':
        return binding.byteLengthUtf8(string);

      case 'ucs2':
      case 'ucs-2':
      case 'utf16le':
      case 'utf-16le':
        return len * 2;

      case 'hex':
        return len >>> 1;

      case 'base64':
        return base64ByteLength(string, len);

      default:
        // The C++ binding defaulted to UTF8, we should too.
        if (loweredCase)
          return binding.byteLengthUtf8(string);

        encoding = ('' + encoding).toLowerCase();
        loweredCase = true;
    }
  }
}

Buffer.byteLength = byteLength;


// For backwards compatibility.
Object.defineProperty(Buffer.prototype, 'parent', {
  enumerable: true,
  get: function() {
    if (!(this instanceof Buffer))
      return undefined;
    if (this.byteLength === 0 ||
        this.byteLength === this.buffer.byteLength) {
      return undefined;
    }
    return this.buffer;
  }
});
Object.defineProperty(Buffer.prototype, 'offset', {
  enumerable: true,
  get: function() {
    if (!(this instanceof Buffer))
      return undefined;
    return this.byteOffset;
  }
});


function slowToString(encoding, start, end) {
  var loweredCase = false;

  start = start >>> 0;
  end = end === undefined || end === Infinity ? this.length : end >>> 0;

  if (!encoding) encoding = 'utf8';
  if (start < 0) start = 0;
  if (end > this.length) end = this.length;
  if (end <= start) return '';

  while (true) {
    switch (encoding) {
      case 'hex':
        return this.hexSlice(start, end);

      case 'utf8':
      case 'utf-8':
        return this.utf8Slice(start, end);

      case 'ascii':
        return this.asciiSlice(start, end);

      case 'binary':
        return this.binarySlice(start, end);

      case 'base64':
        return this.base64Slice(start, end);

      case 'ucs2':
      case 'ucs-2':
      case 'utf16le':
      case 'utf-16le':
        return this.ucs2Slice(start, end);

      default:
        if (loweredCase)
          throw new TypeError('Unknown encoding: ' + encoding);
        encoding = (encoding + '').toLowerCase();
        loweredCase = true;
    }
  }
}


Buffer.prototype.toString = function() {
  if (arguments.length === 0) {
    var result = this.utf8Slice(0, this.length);
  } else {
    var result = slowToString.apply(this, arguments);
  }
  if (result === undefined)
    throw new Error('toString failed');
  return result;
};


Buffer.prototype.equals = function equals(b) {
  if (!(b instanceof Buffer))
    throw new TypeError('Argument must be a Buffer');

  if (this === b)
    return true;

  return binding.compare(this, b) === 0;
};


// Inspect
Buffer.prototype.inspect = function inspect() {
  var str = '';
  var max = exports.INSPECT_MAX_BYTES;
  if (this.length > 0) {
    str = this.toString('hex', 0, max).match(/.{2}/g).join(' ');
    if (this.length > max)
      str += ' ... ';
  }
  return '<' + this.constructor.name + ' ' + str + '>';
};


Buffer.prototype.compare = function compare(b) {
  if (!(b instanceof Buffer))
    throw new TypeError('Argument must be a Buffer');

  if (this === b)
    return 0;

  return binding.compare(this, b);
};

function slowIndexOf(buffer, val, byteOffset, encoding) {
  var loweredCase = false;
  for (;;) {
    switch (encoding) {
      case 'utf8':
      case 'utf-8':
      case 'ucs2':
      case 'ucs-2':
      case 'utf16le':
      case 'utf-16le':
      case 'binary':
        return binding.indexOfString(buffer, val, byteOffset, encoding);

      case 'base64':
      case 'ascii':
      case 'hex':
        return binding.indexOfBuffer(
            buffer, Buffer(val, encoding), byteOffset, encoding);

      default:
        if (loweredCase) {
          throw new TypeError('Unknown encoding: ' + encoding);
        }

        encoding = ('' + encoding).toLowerCase();
        loweredCase = true;
    }
  }
}

Buffer.prototype.indexOf = function indexOf(val, byteOffset, encoding) {
  if (byteOffset > 0x7fffffff)
    byteOffset = 0x7fffffff;
  else if (byteOffset < -0x80000000)
    byteOffset = -0x80000000;
  byteOffset >>= 0;

  if (typeof val === 'string') {
    if (encoding === undefined) {
      return binding.indexOfString(this, val, byteOffset, encoding);
    }
    return slowIndexOf(this, val, byteOffset, encoding);
  } else if (val instanceof Buffer) {
    return binding.indexOfBuffer(this, val, byteOffset, encoding);
  } else if (typeof val === 'number') {
    return binding.indexOfNumber(this, val, byteOffset);
  }

  throw new TypeError('val must be string, number or Buffer');
};


Buffer.prototype.fill = function fill(val, start, end) {
  start = start >> 0;
  end = (end === undefined) ? this.length : end >> 0;

  if (start < 0 || end > this.length)
    throw new RangeError('out of range index');
  if (end <= start)
    return this;

  if (typeof val !== 'string') {
    val = val >>> 0;
  } else if (val.length === 1) {
    var code = val.charCodeAt(0);
    if (code < 256)
      val = code;
  }

  binding.fill(this, val, start, end);

  return this;
};


// XXX remove in v0.13
Buffer.prototype.get = internalUtil.deprecate(function get(offset) {
  offset = ~~offset;
  if (offset < 0 || offset >= this.length)
    throw new RangeError('index out of range');
  return this[offset];
}, 'Buffer.get is deprecated. Use array indexes instead.');


// XXX remove in v0.13
Buffer.prototype.set = internalUtil.deprecate(function set(offset, v) {
  offset = ~~offset;
  if (offset < 0 || offset >= this.length)
    throw new RangeError('index out of range');
  return this[offset] = v;
}, 'Buffer.set is deprecated. Use array indexes instead.');


// TODO(trevnorris): fix these checks to follow new standard
// write(string, offset = 0, length = buffer.length, encoding = 'utf8')
var writeWarned = false;
const writeMsg = 'Buffer.write(string, encoding, offset, length) is ' +
                 'deprecated. Use write(string[, offset[, length]]' +
                 '[, encoding]) instead.';
Buffer.prototype.write = function(string, offset, length, encoding) {
  // Buffer#write(string);
  if (offset === undefined) {
    encoding = 'utf8';
    length = this.length;
    offset = 0;

  // Buffer#write(string, encoding)
  } else if (length === undefined && typeof offset === 'string') {
    encoding = offset;
    length = this.length;
    offset = 0;

  // Buffer#write(string, offset[, length][, encoding])
  } else if (isFinite(offset)) {
    offset = offset >>> 0;
    if (isFinite(length)) {
      length = length >>> 0;
      if (encoding === undefined)
        encoding = 'utf8';
    } else {
      encoding = length;
      length = undefined;
    }

  // XXX legacy write(string, encoding, offset, length) - remove in v0.13
  } else {
    writeWarned = internalUtil.printDeprecationMessage(writeMsg, writeWarned);
    var swap = encoding;
    encoding = offset;
    offset = length >>> 0;
    length = swap;
  }

  var remaining = this.length - offset;
  if (length === undefined || length > remaining)
    length = remaining;

  if (string.length > 0 && (length < 0 || offset < 0))
    throw new RangeError('attempt to write outside buffer bounds');

  if (!encoding)
    encoding = 'utf8';

  var loweredCase = false;
  for (;;) {
    switch (encoding) {
      case 'hex':
        return this.hexWrite(string, offset, length);

      case 'utf8':
      case 'utf-8':
        return this.utf8Write(string, offset, length);

      case 'ascii':
        return this.asciiWrite(string, offset, length);

      case 'binary':
        return this.binaryWrite(string, offset, length);

      case 'base64':
        // Warning: maxLength not taken into account in base64Write
        return this.base64Write(string, offset, length);

      case 'ucs2':
      case 'ucs-2':
      case 'utf16le':
      case 'utf-16le':
        return this.ucs2Write(string, offset, length);

      default:
        if (loweredCase)
          throw new TypeError('Unknown encoding: ' + encoding);
        encoding = ('' + encoding).toLowerCase();
        loweredCase = true;
    }
  }
};


Buffer.prototype.toJSON = function() {
  return {
    type: 'Buffer',
    data: Array.prototype.slice.call(this, 0)
  };
};


// TODO(trevnorris): currently works like Array.prototype.slice(), which
// doesn't follow the new standard for throwing on out of range indexes.
Buffer.prototype.slice = function slice(start, end) {
  const buffer = this.subarray(start, end);
  Object.setPrototypeOf(buffer, Buffer.prototype);
  return buffer;
};


function checkOffset(offset, ext, length) {
  if (offset + ext > length)
    throw new RangeError('index out of range');
}


Buffer.prototype.readUIntLE = function(offset, byteLength, noAssert) {
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert)
    checkOffset(offset, byteLength, this.length);

  var val = this[offset];
  var mul = 1;
  var i = 0;
  while (++i < byteLength && (mul *= 0x100))
    val += this[offset + i] * mul;

  return val;
};


Buffer.prototype.readUIntBE = function(offset, byteLength, noAssert) {
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert)
    checkOffset(offset, byteLength, this.length);

  var val = this[offset + --byteLength];
  var mul = 1;
  while (byteLength > 0 && (mul *= 0x100))
    val += this[offset + --byteLength] * mul;

  return val;
};


Buffer.prototype.readUInt8 = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 1, this.length);
  return this[offset];
};


Buffer.prototype.readUInt16LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  return this[offset] | (this[offset + 1] << 8);
};


Buffer.prototype.readUInt16BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  return (this[offset] << 8) | this[offset + 1];
};


Buffer.prototype.readUInt32LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);

  return ((this[offset]) |
      (this[offset + 1] << 8) |
      (this[offset + 2] << 16)) +
      (this[offset + 3] * 0x1000000);
};


Buffer.prototype.readUInt32BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);

  return (this[offset] * 0x1000000) +
      ((this[offset + 1] << 16) |
      (this[offset + 2] << 8) |
      this[offset + 3]);
};


Buffer.prototype.readIntLE = function(offset, byteLength, noAssert) {
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert)
    checkOffset(offset, byteLength, this.length);

  var val = this[offset];
  var mul = 1;
  var i = 0;
  while (++i < byteLength && (mul *= 0x100))
    val += this[offset + i] * mul;
  mul *= 0x80;

  if (val >= mul)
    val -= Math.pow(2, 8 * byteLength);

  return val;
};


Buffer.prototype.readIntBE = function(offset, byteLength, noAssert) {
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert)
    checkOffset(offset, byteLength, this.length);

  var i = byteLength;
  var mul = 1;
  var val = this[offset + --i];
  while (i > 0 && (mul *= 0x100))
    val += this[offset + --i] * mul;
  mul *= 0x80;

  if (val >= mul)
    val -= Math.pow(2, 8 * byteLength);

  return val;
};


Buffer.prototype.readInt8 = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 1, this.length);
  var val = this[offset];
  return !(val & 0x80) ? val : (0xff - val + 1) * -1;
};


Buffer.prototype.readInt16LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  var val = this[offset] | (this[offset + 1] << 8);
  return (val & 0x8000) ? val | 0xFFFF0000 : val;
};


Buffer.prototype.readInt16BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  var val = this[offset + 1] | (this[offset] << 8);
  return (val & 0x8000) ? val | 0xFFFF0000 : val;
};


Buffer.prototype.readInt32LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);

  return (this[offset]) |
      (this[offset + 1] << 8) |
      (this[offset + 2] << 16) |
      (this[offset + 3] << 24);
};


Buffer.prototype.readInt32BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);

  return (this[offset] << 24) |
      (this[offset + 1] << 16) |
      (this[offset + 2] << 8) |
      (this[offset + 3]);
};


Buffer.prototype.readFloatLE = function readFloatLE(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return binding.readFloatLE(this, offset);
};


Buffer.prototype.readFloatBE = function readFloatBE(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return binding.readFloatBE(this, offset);
};


Buffer.prototype.readDoubleLE = function readDoubleLE(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 8, this.length);
  return binding.readDoubleLE(this, offset);
};


Buffer.prototype.readDoubleBE = function readDoubleBE(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 8, this.length);
  return binding.readDoubleBE(this, offset);
};


function checkInt(buffer, value, offset, ext, max, min) {
  if (!(buffer instanceof Buffer))
    throw new TypeError('buffer must be a Buffer instance');
  if (value > max || value < min)
    throw new TypeError('value is out of bounds');
  if (offset + ext > buffer.length)
    throw new RangeError('index out of range');
}


Buffer.prototype.writeUIntLE = function(value, offset, byteLength, noAssert) {
  value = +value;
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert) {
    const maxBytes = Math.pow(2, 8 * byteLength) - 1;
    checkInt(this, value, offset, byteLength, maxBytes, 0);
  }

  var mul = 1;
  var i = 0;
  this[offset] = value;
  while (++i < byteLength && (mul *= 0x100))
    this[offset + i] = (value / mul) >>> 0;

  return offset + byteLength;
};


Buffer.prototype.writeUIntBE = function(value, offset, byteLength, noAssert) {
  value = +value;
  offset = offset >>> 0;
  byteLength = byteLength >>> 0;
  if (!noAssert) {
    const maxBytes = Math.pow(2, 8 * byteLength) - 1;
    checkInt(this, value, offset, byteLength, maxBytes, 0);
  }

  var i = byteLength - 1;
  var mul = 1;
  this[offset + i] = value;
  while (--i >= 0 && (mul *= 0x100))
    this[offset + i] = (value / mul) >>> 0;

  return offset + byteLength;
};


Buffer.prototype.writeUInt8 = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 1, 0xff, 0);
  this[offset] = value;
  return offset + 1;
};


Buffer.prototype.writeUInt16LE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0xffff, 0);
  this[offset] = value;
  this[offset + 1] = (value >>> 8);
  return offset + 2;
};


Buffer.prototype.writeUInt16BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0xffff, 0);
  this[offset] = (value >>> 8);
  this[offset + 1] = value;
  return offset + 2;
};


Buffer.prototype.writeUInt32LE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0xffffffff, 0);
  this[offset + 3] = (value >>> 24);
  this[offset + 2] = (value >>> 16);
  this[offset + 1] = (value >>> 8);
  this[offset] = value;
  return offset + 4;
};


Buffer.prototype.writeUInt32BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0xffffffff, 0);
  this[offset] = (value >>> 24);
  this[offset + 1] = (value >>> 16);
  this[offset + 2] = (value >>> 8);
  this[offset + 3] = value;
  return offset + 4;
};


Buffer.prototype.writeIntLE = function(value, offset, byteLength, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert) {
    checkInt(this,
             value,
             offset,
             byteLength,
             Math.pow(2, 8 * byteLength - 1) - 1,
             -Math.pow(2, 8 * byteLength - 1));
  }

  var i = 0;
  var mul = 1;
  var sub = value < 0 ? 1 : 0;
  this[offset] = value;
  while (++i < byteLength && (mul *= 0x100))
    this[offset + i] = ((value / mul) >> 0) - sub;

  return offset + byteLength;
};


Buffer.prototype.writeIntBE = function(value, offset, byteLength, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert) {
    checkInt(this,
             value,
             offset,
             byteLength,
             Math.pow(2, 8 * byteLength - 1) - 1,
             -Math.pow(2, 8 * byteLength - 1));
  }

  var i = byteLength - 1;
  var mul = 1;
  var sub = value < 0 ? 1 : 0;
  this[offset + i] = value;
  while (--i >= 0 && (mul *= 0x100))
    this[offset + i] = ((value / mul) >> 0) - sub;

  return offset + byteLength;
};


Buffer.prototype.writeInt8 = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 1, 0x7f, -0x80);
  this[offset] = value;
  return offset + 1;
};


Buffer.prototype.writeInt16LE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0x7fff, -0x8000);
  this[offset] = value;
  this[offset + 1] = (value >>> 8);
  return offset + 2;
};


Buffer.prototype.writeInt16BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0x7fff, -0x8000);
  this[offset] = (value >>> 8);
  this[offset + 1] = value;
  return offset + 2;
};


Buffer.prototype.writeInt32LE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000);
  this[offset] = value;
  this[offset + 1] = (value >>> 8);
  this[offset + 2] = (value >>> 16);
  this[offset + 3] = (value >>> 24);
  return offset + 4;
};


Buffer.prototype.writeInt32BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000);
  this[offset] = (value >>> 24);
  this[offset + 1] = (value >>> 16);
  this[offset + 2] = (value >>> 8);
  this[offset + 3] = value;
  return offset + 4;
};


function checkFloat(buffer, value, offset, ext) {
  if (!(buffer instanceof Buffer))
    throw new TypeError('buffer must be a Buffer instance');
  if (offset + ext > buffer.length)
    throw new RangeError('index out of range');
}


Buffer.prototype.writeFloatLE = function writeFloatLE(val, offset, noAssert) {
  val = +val;
  offset = offset >>> 0;
  if (!noAssert)
    checkFloat(this, val, offset, 4);
  binding.writeFloatLE(this, val, offset);
  return offset + 4;
};


Buffer.prototype.writeFloatBE = function writeFloatBE(val, offset, noAssert) {
  val = +val;
  offset = offset >>> 0;
  if (!noAssert)
    checkFloat(this, val, offset, 4);
  binding.writeFloatBE(this, val, offset);
  return offset + 4;
};


Buffer.prototype.writeDoubleLE = function writeDoubleLE(val, offset, noAssert) {
  val = +val;
  offset = offset >>> 0;
  if (!noAssert)
    checkFloat(this, val, offset, 8);
  binding.writeDoubleLE(this, val, offset);
  return offset + 8;
};


Buffer.prototype.writeDoubleBE = function writeDoubleBE(val, offset, noAssert) {
  val = +val;
  offset = offset >>> 0;
  if (!noAssert)
    checkFloat(this, val, offset, 8);
  binding.writeDoubleBE(this, val, offset);
  return offset + 8;
};
