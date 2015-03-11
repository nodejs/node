'use strict';

const binding = process.binding('buffer');
const smalloc = process.binding('smalloc');
const util = require('util');
const alloc = smalloc.alloc;
const truncate = smalloc.truncate;
const sliceOnto = smalloc.sliceOnto;
const kMaxLength = smalloc.kMaxLength;

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

function Buffer(arg) {
  if (!(this instanceof Buffer)) {
    // Avoid going through an ArgumentsAdaptorTrampoline in the common case.
    if (arguments.length > 1)
      return new Buffer(arg, arguments[1]);

    return new Buffer(arg);
  }

  this.length = 0;
  this.parent = undefined;

  // Common case.
  if (typeof(arg) === 'number') {
    fromNumber(this, arg);
    return;
  }

  // Slightly less common case.
  if (typeof(arg) === 'string') {
    fromString(this, arg, arguments.length > 1 ? arguments[1] : 'utf8');
    return;
  }

  // Unusual.
  fromObject(this, arg);
}

function fromNumber(that, length) {
  allocate(that, length < 0 ? 0 : checked(length) | 0);
}

function fromString(that, string, encoding) {
  if (typeof(encoding) !== 'string' || encoding === '')
    encoding = 'utf8';

  // Assumption: byteLength() return value is always < kMaxLength.
  var length = byteLength(string, encoding) | 0;
  allocate(that, length);

  var actual = that.write(string, encoding) | 0;
  if (actual !== length) {
    // Fix up for truncated base64 input.  Don't bother returning
    // the unused two or three bytes to the pool.
    that.length = actual;
    truncate(that, actual);
  }
}

function fromObject(that, object) {
  if (object instanceof Buffer)
    return fromBuffer(that, object);

  if (Array.isArray(object))
    return fromArray(that, object);

  if (object == null)
    throw new TypeError('must start with number, buffer, array or string');

  if (object.buffer instanceof ArrayBuffer)
    return fromTypedArray(that, object);

  if (object.length)
    return fromArrayLike(that, object);

  return fromJsonObject(that, object);
}

function fromBuffer(that, buffer) {
  var length = checked(buffer.length) | 0;
  allocate(that, length);
  buffer.copy(that, 0, 0, length);
}

function fromArray(that, array) {
  var length = checked(array.length) | 0;
  allocate(that, length);
  for (var i = 0; i < length; i += 1)
    that[i] = array[i] & 255;
}

// Duplicate of fromArray() to keep fromArray() monomorphic.
function fromTypedArray(that, array) {
  var length = checked(array.length) | 0;
  allocate(that, length);
  // Truncating the elements is probably not what people expect from typed
  // arrays with BYTES_PER_ELEMENT > 1 but it's compatible with the behavior
  // of the old Buffer constructor.
  for (var i = 0; i < length; i += 1)
    that[i] = array[i] & 255;
}

function fromArrayLike(that, array) {
  var length = checked(array.length) | 0;
  allocate(that, length);
  for (var i = 0; i < length; i += 1)
    that[i] = array[i] & 255;
}

// Deserialize { type: 'Buffer', data: [1,2,3,...] } into a Buffer object.
// Returns a zero-length buffer for inputs that don't conform to the spec.
function fromJsonObject(that, object) {
  var array;
  var length = 0;

  if (object.type === 'Buffer' && Array.isArray(object.data)) {
    array = object.data;
    length = checked(array.length) | 0;
  }
  allocate(that, length);

  for (var i = 0; i < length; i += 1)
    that[i] = array[i] & 255;
}

function allocate(that, length) {
  var fromPool = length !== 0 && length <= Buffer.poolSize >>> 1;
  if (fromPool)
    that.parent = palloc(that, length);
  else
    alloc(that, length);
  that.length = length;
}

function palloc(that, length) {
  if (length > poolSize - poolOffset)
    createPool();

  var start = poolOffset;
  var end = start + length;
  var buf = sliceOnto(allocPool, that, start, end);
  poolOffset = end;

  // Ensure aligned slices
  if (poolOffset & 0x7) {
    poolOffset |= 0x7;
    poolOffset++;
  }

  return buf;
}

function checked(length) {
  // Note: cannot use `length < kMaxLength` here because that fails when
  // length is NaN (which is otherwise coerced to zero.)
  if (length >= kMaxLength) {
    throw new RangeError('Attempt to allocate Buffer larger than maximum ' +
                         'size: 0x' + kMaxLength.toString(16) + ' bytes');
  }
  return length >>> 0;
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
binding.setupBufferJS(NativeBuffer);


// Static methods

Buffer.isBuffer = function isBuffer(b) {
  return b instanceof Buffer;
};


Buffer.compare = function compare(a, b) {
  if (!(a instanceof Buffer) ||
      !(b instanceof Buffer))
    throw new TypeError('Arguments must be Buffers');

  if (a === b)
    return 0;

  return binding.compare(a, b);
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
  if (!Array.isArray(list))
    throw new TypeError('list argument must be an Array of Buffers.');

  if (length === undefined) {
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


function byteLength(string, encoding) {
  if (typeof(string) !== 'string')
    string = String(string);

  switch (encoding) {
    case 'ascii':
    case 'binary':
    case 'raw':
      return string.length;

    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      return string.length * 2;

    case 'hex':
      return string.length >>> 1;
  }

  return binding.byteLength(string, encoding);
}

Buffer.byteLength = byteLength;

// toString(encoding, start=0, end=buffer.length)
Buffer.prototype.toString = function(encoding, start, end) {
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


Buffer.prototype.indexOf = function indexOf(val, byteOffset) {
  if (byteOffset > 0x7fffffff)
    byteOffset = 0x7fffffff;
  else if (byteOffset < -0x80000000)
    byteOffset = -0x80000000;
  byteOffset >>= 0;

  if (typeof val === 'string')
    return binding.indexOfString(this, val, byteOffset);
  if (val instanceof Buffer)
    return binding.indexOfBuffer(this, val, byteOffset);
  if (typeof val === 'number')
    return binding.indexOfNumber(this, val, byteOffset);

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
const writeMsg = '.write(string, encoding, offset, length) is deprecated.' +
                 ' Use write(string[, offset[, length]][, encoding]) instead.';
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
Buffer.prototype.slice = function(start, end) {
  var len = this.length;
  start = ~~start;
  end = end === undefined ? len : ~~end;

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
    buf.parent = this.parent === undefined ? this : this.parent;

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

// ES6 iterator

var ITERATOR_KIND_KEYS = 1;
var ITERATOR_KIND_ENTRIES = 3;

function BufferIteratorResult(value, done) {
  this.value = value;
  this.done = done;
}

var resultCache = new Array(256);

for (var i = 0; i < 256; i++)
  resultCache[i] = Object.freeze(new BufferIteratorResult(i, false));

var finalResult = Object.freeze(new BufferIteratorResult(undefined, true));

function BufferIterator(buffer, kind) {
  this._buffer = buffer;
  this._kind = kind;
  this._index = 0;
}

BufferIterator.prototype.next = function() {
  var buffer = this._buffer;
  var kind = this._kind;
  var index = this._index;

  if (index >= buffer.length)
    return finalResult;

  this._index++;

  if (kind === ITERATOR_KIND_ENTRIES)
    return new BufferIteratorResult([index, buffer[index]], false);

  return new BufferIteratorResult(index, false);
};

function BufferValueIterator(buffer) {
  BufferIterator.call(this, buffer, null);
}

BufferValueIterator.prototype.next = function() {
  var buffer = this._buffer;
  var index = this._index;

  if (index >= buffer.length)
    return finalResult;

  this._index++;

  return resultCache[buffer[index]];
};


BufferIterator.prototype[Symbol.iterator] = function() {
  return this;
};

BufferValueIterator.prototype[Symbol.iterator] =
    BufferIterator.prototype[Symbol.iterator];

Buffer.prototype.keys = function() {
  return new BufferIterator(this, ITERATOR_KIND_KEYS);
};

Buffer.prototype.entries = function() {
  return new BufferIterator(this, ITERATOR_KIND_ENTRIES);
};

Buffer.prototype.values = function() {
  return new BufferValueIterator(this);
};

Buffer.prototype[Symbol.iterator] = Buffer.prototype.values;
