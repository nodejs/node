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

  if (util.isNumber(subject))
    this.length = subject > 0 ? subject >>> 0 : 0;
  else if (util.isString(subject))
    this.length = Buffer.byteLength(subject, encoding = encoding || 'utf8');
  else if (util.isObject(subject))
    this.length = +subject.length > 0 ? Math.floor(+subject.length) : 0;
  else
    throw new TypeError('must start with number, buffer, array or string');

  if (this.length > kMaxLength) {
    throw new RangeError('Attempt to allocate Buffer larger than maximum ' +
                         'size: 0x' + kMaxLength.toString(16) + ' bytes');
  }

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

  if (!util.isNumber(subject)) {
    if (util.isString(subject)) {
      // In the case of base64 it's possible that the size of the buffer
      // allocated was slightly too large. In this case we need to rewrite
      // the length to the actual length written.
      this.length = this.write(subject, encoding);
    } else {
      if (util.isBuffer(subject))
        subject.copy(this, 0, 0, this.length);
      else if (util.isNumber(subject.length) || util.isArray(subject))
        for (var i = 0; i < this.length; i++)
          this[i] = subject[i];
    }
  }
}


function SlowBuffer(length) {
  length = length >>> 0;
  if (this.length > kMaxLength) {
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
  this.length = length;
}
NativeBuffer.prototype = Buffer.prototype;


// add methods to Buffer prototype
buffer.setupBufferJS(NativeBuffer, internal);


// Static methods

Buffer.isBuffer = function isBuffer(b) {
  return util.isBuffer(b);
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


// pre-set for values that may exist in the future
Buffer.prototype.length = undefined;
Buffer.prototype.parent = undefined;


// toString(encoding, start=0, end=buffer.length)
Buffer.prototype.toString = function(encoding, start, end) {
  var loweredCase = false;

  start = start >>> 0;
  end = util.isUndefined(end) ? this.length : end >>> 0;

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


Buffer.prototype.readUInt8 = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 1, this.length);
  return this[offset];
};


function readUInt16(buffer, offset, isBigEndian) {
  var val = 0;
  if (isBigEndian) {
    val = buffer[offset] << 8;
    val |= buffer[offset + 1];
  } else {
    val = buffer[offset];
    val |= buffer[offset + 1] << 8;
  }
  return val;
}


Buffer.prototype.readUInt16LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  return readUInt16(this, offset, false, noAssert);
};


Buffer.prototype.readUInt16BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  return readUInt16(this, offset, true, noAssert);
};


function readUInt32(buffer, offset, isBigEndian) {
  var val = 0;
  if (isBigEndian) {
    val = buffer[offset + 1] << 16;
    val |= buffer[offset + 2] << 8;
    val |= buffer[offset + 3];
    val = val + (buffer[offset] << 24 >>> 0);
  } else {
    val = buffer[offset + 2] << 16;
    val |= buffer[offset + 1] << 8;
    val |= buffer[offset];
    val = val + (buffer[offset + 3] << 24 >>> 0);
  }
  return val;
}


Buffer.prototype.readUInt32LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return readUInt32(this, offset, false);
};


Buffer.prototype.readUInt32BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return readUInt32(this, offset, true);
};


/*
 * Signed integer types, yay team! A reminder on how two's complement actually
 * works. The first bit is the signed bit, i.e. tells us whether or not the
 * number should be positive or negative. If the two's complement value is
 * positive, then we're done, as it's equivalent to the unsigned representation.
 *
 * Now if the number is positive, you're pretty much done, you can just leverage
 * the unsigned translations and return those. Unfortunately, negative numbers
 * aren't quite that straightforward.
 *
 * At first glance, one might be inclined to use the traditional formula to
 * translate binary numbers between the positive and negative values in two's
 * complement. (Though it doesn't quite work for the most negative value)
 * Mainly:
 *  - invert all the bits
 *  - add one to the result
 *
 * Of course, this doesn't quite work in Javascript. Take for example the value
 * of -128. This could be represented in 16 bits (big-endian) as 0xff80. But of
 * course, Javascript will do the following:
 *
 * > ~0xff80
 * -65409
 *
 * Whoh there, Javascript, that's not quite right. But wait, according to
 * Javascript that's perfectly correct. When Javascript ends up seeing the
 * constant 0xff80, it has no notion that it is actually a signed number. It
 * assumes that we've input the unsigned value 0xff80. Thus, when it does the
 * binary negation, it casts it into a signed value, (positive 0xff80). Then
 * when you perform binary negation on that, it turns it into a negative number.
 *
 * Instead, we're going to have to use the following general formula, that works
 * in a rather Javascript friendly way. I'm glad we don't support this kind of
 * weird numbering scheme in the kernel.
 *
 * (BIT-MAX - (unsigned)val + 1) * -1
 *
 * The astute observer, may think that this doesn't make sense for 8-bit numbers
 * (really it isn't necessary for them). However, when you get 16-bit numbers,
 * you do. Let's go back to our prior example and see how this will look:
 *
 * (0xffff - 0xff80 + 1) * -1
 * (0x007f + 1) * -1
 * (0x0080) * -1
 */

Buffer.prototype.readInt8 = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 1, this.length);
  var val = this[offset];
  return !(val & 0x80) ? val : (0xff - val + 1) * -1;
};


function readInt16(buffer, offset, isBigEndian) {
  var val = readUInt16(buffer, offset, isBigEndian);
  return !(val & 0x8000) ? val : (0xffff - val + 1) * -1;
}


Buffer.prototype.readInt16LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  return readInt16(this, offset, false);
};


Buffer.prototype.readInt16BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  return readInt16(this, offset, true);
};


function readInt32(buffer, offset, isBigEndian) {
  var val = readUInt32(buffer, offset, isBigEndian);
  return !(val & 0x80000000) ? val : (0xffffffff - val + 1) * -1;
}


Buffer.prototype.readInt32LE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return readInt32(this, offset, false);
};


Buffer.prototype.readInt32BE = function(offset, noAssert) {
  offset = offset >>> 0;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return readInt32(this, offset, true);
};


function checkInt(buffer, value, offset, ext, max, min) {
  if (!(buffer instanceof Buffer))
    throw new TypeError('buffer must be a Buffer instance');
  if (value > max || value < min)
    throw new TypeError('value is out of bounds');
  if (offset + ext > buffer.length)
    throw new RangeError('index out of range');
}


Buffer.prototype.writeUInt8 = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 1, 0xff, 0);
  this[offset] = value;
  return offset + 1;
};


function writeUInt16(buffer, value, offset, isBigEndian) {
  if (isBigEndian) {
    buffer[offset] = (value & 0xff00) >>> 8;
    buffer[offset + 1] = value & 0x00ff;
  } else {
    buffer[offset + 1] = (value & 0xff00) >>> 8;
    buffer[offset] = value & 0x00ff;
  }
  return offset + 2;
}


Buffer.prototype.writeUInt16LE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0xffff, 0);
  return writeUInt16(this, value, offset, false);
};


Buffer.prototype.writeUInt16BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0xffff, 0);
  return writeUInt16(this, value, offset, true);
};


function writeUInt32(buffer, value, offset, isBigEndian) {
  if (isBigEndian) {
    buffer[offset] = (value >>> 24) & 0xff;
    buffer[offset + 1] = (value >>> 16) & 0xff;
    buffer[offset + 2] = (value >>> 8) & 0xff;
    buffer[offset + 3] = value & 0xff;
  } else {
    buffer[offset + 3] = (value >>> 24) & 0xff;
    buffer[offset + 2] = (value >>> 16) & 0xff;
    buffer[offset + 1] = (value >>> 8) & 0xff;
    buffer[offset] = value & 0xff;
  }
  return offset + 4;
}


Buffer.prototype.writeUInt32LE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0xffffffff, 0);
  return writeUInt32(this, value, offset, false);
};


Buffer.prototype.writeUInt32BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0xffffffff, 0);
  return writeUInt32(this, value, offset, true);
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
  return writeUInt16(this, value, offset, false);
};


Buffer.prototype.writeInt16BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0x7fff, -0x8000);
  return writeUInt16(this, value, offset, true);
};


Buffer.prototype.writeInt32LE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000);
  return writeUInt32(this, value, offset, false);
};


Buffer.prototype.writeInt32BE = function(value, offset, noAssert) {
  value = +value;
  offset = offset >>> 0;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000);
  return writeUInt32(this, value, offset, true);
};
