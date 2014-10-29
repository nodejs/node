// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var buffer = process.binding('buffer');
var smalloc = process.binding('smalloc');
var util = require('util');
var alloc = smalloc.alloc;
var truncate = smalloc.truncate;
var sliceOnto = smalloc.sliceOnto;
var kMaxLength = smalloc.kMaxLength;
var internal = {};

exports.Buffer = Buffer;
exports.SlowBuffer = SlowBuffer;
exports.INSPECT_MAX_BYTES = 50;


Buffer.poolSize = 8 * 1024;
var poolSize, poolOffset, allocPool;


function createPool() {
  poolSize = Buffer.poolSize;
  allocPool = alloc({}, poolSize);
  poolOffset = 0;
}
createPool();


function Buffer(subject, encoding) {
  if (!util.isBuffer(this))
    return new Buffer(subject, encoding);

  if (util.isNumber(subject)) {
    this.length = subject > 0 ? subject >>> 0 : 0;

  } else if (util.isString(subject)) {
    if (!util.isString(encoding) || encoding.length === 0)
      encoding = 'utf8';
    this.length = Buffer.byteLength(subject, encoding);

  // Handle Arrays, Buffers, Uint8Arrays or JSON.
  } else if (util.isObject(subject)) {
    if (subject.type === 'Buffer' && util.isArray(subject.data))
      subject = subject.data;
    // Must use floor() because array length may be > kMaxLength.
    this.length = +subject.length > 0 ? Math.floor(+subject.length) : 0;

  } else {
    throw new TypeError('must start with number, buffer, array or string');
  }

  if (this.length > kMaxLength) {
    throw new RangeError('Attempt to allocate Buffer larger than maximum ' +
                         'size: 0x' + kMaxLength.toString(16) + ' bytes');
  }

  this.parent = undefined;
  if (this.length <= (Buffer.poolSize >>> 1) && this.length > 0) {
    if (this.length > poolSize - poolOffset)
      createPool();
    this.parent = sliceOnto(allocPool,
                            this,
                            poolOffset,
                            poolOffset + this.length);
    poolOffset += this.length;
  } else {
    alloc(this, this.length);
  }

  if (util.isNumber(subject)) {
    return;
  }

  if (util.isString(subject)) {
    // In the case of base64 it's possible that the size of the buffer
    // allocated was slightly too large. In this case we need to rewrite
    // the length to the actual length written.
    var len = this.write(subject, encoding);
    // Buffer was truncated after decode, realloc internal ExternalArray
    if (len !== this.length) {
      var prevLen = this.length;
      this.length = len;
      truncate(this, this.length);
      poolOffset -= (prevLen - len);
    }

  } else if (util.isBuffer(subject)) {
    subject.copy(this, 0, 0, this.length);

  } else if (util.isNumber(subject.length) || util.isArray(subject)) {
    // Really crappy way to handle Uint8Arrays, but V8 doesn't give a simple
    // way to access the data from the C++ API.
    for (var i = 0; i < this.length; i++)
      this[i] = subject[i];
  }
}


function SlowBuffer(length) {
  length = length >>> 0;
  if (length > kMaxLength) {
    throw new RangeError('Attempt to allocate Buffer larger than maximum ' +
                         'size: 0x' + kMaxLength.toString(16) + ' bytes');
  }
  var b = new NativeBuffer(length);
  alloc(b, length);
  return b;
}


// Bypass all checks for instantiating unallocated Buffer required for
// Objects created in C++. Significantly faster than calling the Buffer
// function.
function NativeBuffer(length) {
  this.length = length >>> 0;
  // Set this to keep the object map the same.
  this.parent = undefined;
}
NativeBuffer.prototype = Buffer.prototype;


// add methods to Buffer prototype
buffer.setupBufferJS(NativeBuffer, internal);


// Static methods

Buffer.isBuffer = function isBuffer(b) {
  return util.isBuffer(b);
};


Buffer.compare = function compare(a, b) {
  if (!(a instanceof Buffer) ||
      !(b instanceof Buffer))
    throw new TypeError('Arguments must be Buffers');

  return internal.compare(a, b);
};


Buffer.isEncoding = function(encoding) {
  switch ((encoding + '').toLowerCase()) {
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
    case 'raw':
      return true;

    default:
      return false;
  }
};


Buffer.concat = function(list, length) {
  if (!util.isArray(list))
    throw new TypeError('Usage: Buffer.concat(list[, length])');

  if (util.isUndefined(length)) {
    length = 0;
    for (var i = 0; i < list.length; i++)
      length += list[i].length;
  } else {
    length = length >>> 0;
  }

  if (list.length === 0)
    return new Buffer(0);
  else if (list.length === 1)
    return list[0];

  var buffer = new Buffer(length);
  var pos = 0;
  for (var i = 0; i < list.length; i++) {
    var buf = list[i];
    buf.copy(buffer, pos);
    pos += buf.length;
  }

  return buffer;
};


Buffer.byteLength = function(str, enc) {
  var ret;
  str = str + '';
  switch (enc) {
    case 'ascii':
    case 'binary':
    case 'raw':
      ret = str.length;
      break;
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      ret = str.length * 2;
      break;
    case 'hex':
      ret = str.length >>> 1;
      break;
    default:
      ret = internal.byteLength(str, enc);
  }
  return ret;
};


// toString(encoding, start=0, end=buffer.length)
Buffer.prototype.toString = function(encoding, start, end) {
  var loweredCase = false;

  start = start >>> 0;
  end = util.isUndefined(end) || end === Infinity ? this.length : end >>> 0;

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
};


Buffer.prototype.equals = function equals(b) {
  if (!(b instanceof Buffer))
    throw new TypeError('Argument must be a Buffer');

  return internal.compare(this, b) === 0;
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

  return internal.compare(this, b);
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

  internal.fill(this, val, start, end);

  return this;
};


// XXX remove in v0.13
Buffer.prototype.get = util.deprecate(function get(offset) {
  offset = ~~offset;
  if (offset < 0 || offset >= this.length)
    throw new RangeError('index out of range');
  return this[offset];
}, '.get() is deprecated. Access using array indexes instead.');


// XXX remove in v0.13
Buffer.prototype.set = util.deprecate(function set(offset, v) {
  offset = ~~offset;
  if (offset < 0 || offset >= this.length)
    throw new RangeError('index out of range');
  return this[offset] = v;
}, '.set() is deprecated. Set using array indexes instead.');


// TODO(trevnorris): fix these checks to follow new standard
// write(string, offset = 0, length = buffer.length, encoding = 'utf8')
var writeWarned = false;
var writeMsg = '.write(string, encoding, offset, length) is deprecated.' +
               ' Use write(string[, offset[, length]][, encoding]) instead.';
Buffer.prototype.write = function(string, offset, length, encoding) {
  // Buffer#write(string);
  if (util.isUndefined(offset)) {
    encoding = 'utf8';
    length = this.length;
    offset = 0;

  // Buffer#write(string, encoding)
  } else if (util.isUndefined(length) && util.isString(offset)) {
    encoding = offset;
    length = this.length;
    offset = 0;

  // Buffer#write(string, offset[, length][, encoding])
  } else if (isFinite(offset)) {
    offset = offset >>> 0;
    if (isFinite(length)) {
      length = length >>> 0;
      if (util.isUndefined(encoding))
        encoding = 'utf8';
    } else {
      encoding = length;
      length = undefined;
    }

  // XXX legacy write(string, encoding, offset, length) - remove in v0.13
  } else {
    if (!writeWarned) {
      if (process.throwDeprecation)
        throw new Error(writeMsg);
      else if (process.traceDeprecation)
        console.trace(writeMsg);
      else
        console.error(writeMsg);
      writeWarned = true;
    }

    var swap = encoding;
    encoding = offset;
    offset = length >>> 0;
    length = swap;
  }

  var remaining = this.length - offset;
  if (util.isUndefined(length) || length > remaining)
    length = remaining;

  encoding = !!encoding ? (encoding + '').toLowerCase() : 'utf8';

  if (string.length > 0 && (length < 0 || offset < 0))
    throw new RangeError('attempt to write outside buffer bounds');

  var ret;
  switch (encoding) {
    case 'hex':
      ret = this.hexWrite(string, offset, length);
      break;

    case 'utf8':
    case 'utf-8':
      ret = this.utf8Write(string, offset, length);
      break;

    case 'ascii':
      ret = this.asciiWrite(string, offset, length);
      break;

    case 'binary':
      ret = this.binaryWrite(string, offset, length);
      break;

    case 'base64':
      // Warning: maxLength not taken into account in base64Write
      ret = this.base64Write(string, offset, length);
      break;

    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      ret = this.ucs2Write(string, offset, length);
      break;

    default:
      throw new TypeError('Unknown encoding: ' + encoding);
  }

  return ret;
};


Buffer.prototype.toJSON = function() {
  return {
    type: 'Buffer',
    data: Array.prototype.slice.call(this, 0)
  };
};


// TODO(trevnorris): currently works like Array.prototype.slice(), which
// doesn't follow the new standard for throwing on out of range indexes.
Buffer.prototype.slice = function(start, end) {
  var len = this.length;
  start = ~~start;
  end = util.isUndefined(end) ? len : ~~end;

  if (start < 0) {
    start += len;
    if (start < 0)
      start = 0;
  } else if (start > len) {
    start = len;
  }

  if (end < 0) {
    end += len;
    if (end < 0)
      end = 0;
  } else if (end > len) {
    end = len;
  }

  if (end < start)
    end = start;

  var buf = new NativeBuffer();
  sliceOnto(this, buf, start, end);
  buf.length = end - start;
  if (buf.length > 0)
    buf.parent = util.isUndefined(this.parent) ? this : this.parent;

  return buf;
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
  return internal.readFloatLE(this, offset);
};


Buffer.prototype.readFloatBE = function readFloatBE(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return internal.readFloatBE(this, offset);
};


Buffer.prototype.readDoubleLE = function readDoubleLE(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 8, this.length);
  return internal.readDoubleLE(this, offset);
};


Buffer.prototype.readDoubleBE = function readDoubleBE(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 8, this.length);
  return internal.readDoubleBE(this, offset);
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
  if (!noAssert)
    checkInt(this, value, offset, byteLength, Math.pow(2, 8 * byteLength), 0);

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
  if (!noAssert)
    checkInt(this, value, offset, byteLength, Math.pow(2, 8 * byteLength), 0);

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
  internal.writeFloatLE(this, val, offset);
  return offset + 4;
};


Buffer.prototype.writeFloatBE = function writeFloatBE(val, offset, noAssert) {
  val = +val;
  offset = offset >>> 0;
  if (!noAssert)
    checkFloat(this, val, offset, 4);
  internal.writeFloatBE(this, val, offset);
  return offset + 4;
};


Buffer.prototype.writeDoubleLE = function writeDoubleLE(val, offset, noAssert) {
  val = +val;
  offset = offset >>> 0;
  if (!noAssert)
    checkFloat(this, val, offset, 8);
  internal.writeDoubleLE(this, val, offset);
  return offset + 8;
};


Buffer.prototype.writeDoubleBE = function writeDoubleBE(val, offset, noAssert) {
  val = +val;
  offset = offset >>> 0;
  if (!noAssert)
    checkFloat(this, val, offset, 8);
  internal.writeDoubleBE(this, val, offset);
  return offset + 8;
};
