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

exports.Buffer = Buffer;
exports.SlowBuffer = SlowBuffer;
exports.INSPECT_MAX_BYTES = 50;

// add methods to Buffer prototype
buffer.setupBufferJS(Buffer);

Buffer.poolSize = 8 * 1024;
var poolSize = Buffer.poolSize;
var poolOffset = 0;
var allocPool = alloc({}, poolSize);


function createPool() {
  poolSize = Buffer.poolSize;
  allocPool = alloc({}, poolSize);
  poolOffset = 0;
}


function Buffer(subject, encoding) {
  if (!util.isBuffer(this))
    return new Buffer(subject, encoding);

  if (util.isNumber(subject))
    this.length = subject > 0 ? Math.floor(subject) : 0;
  else if (util.isString(subject))
    this.length = Buffer.byteLength(subject, encoding = encoding || 'utf8');
  else if (util.isObject(subject))
    this.length = +subject.length > 0 ? Math.floor(+subject.length) : 0;
  else if (util.isUndefined(subject)) {
    // undef first arg returns unallocated buffer, also assumes length passed.
    // this is a stop-gap for now while look for better architecture.
    // for internal use only.
    this.length = encoding;
    return;
  }
  else
    throw new TypeError('must start with number, buffer, array or string');

  if (this.length > kMaxLength)
    throw new RangeError('length > kMaxLength');

  if (this.length < Buffer.poolSize / 2 && this.length > 0) {
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
      // FIXME: the number of bytes hasn't changed, so why change the length?
      this.length = this.write(subject, 0, encoding);
    } else {
      if (util.isBuffer(subject))
        this.copy(subject, 0, 0, this.length);
      else if (util.isNumber(subject.length) || util.isArray(subject))
        for (var i = 0; i < this.length; i++)
          this[i] = subject[i];
    }
  }
}


function SlowBuffer(length) {
  length = ~~length;
  var b = new Buffer(undefined, length);
  alloc(b, length);
  return b;
}


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
    length = ~~length;
  }

  if (length < 0) length = 0;

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


// pre-set for values that may exist in the future
Buffer.prototype.length = undefined;
Buffer.prototype.parent = undefined;


// toString(encoding, start=0, end=buffer.length)
Buffer.prototype.toString = function(encoding, start, end) {
  encoding = !!encoding ? (encoding + '').toLowerCase() : 'utf8';

  start = ~~start;
  end = util.isUndefined(end) ? this.length : ~~end;

  if (start < 0) start = 0;
  if (end > this.length) end = this.length;
  if (end <= start) return '';

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
      throw new TypeError('Unknown encoding: ' + encoding);
  }
};


// Inspect
Buffer.prototype.inspect = function inspect() {
  var str = '';
  if (this.length > 0)
    str = this.hexSlice(0, this.length).match(/.{2}/g).join(' ');
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
               ' Use write(string, offset, length, encoding) instead.';
Buffer.prototype.write = function(string, offset, length, encoding) {
  // allow write(string, encoding)
  if (util.isString(offset) && util.isUndefined(length)) {
    encoding = offset;
    offset = 0;

  // allow write(string, offset[, length], encoding)
  } else if (isFinite(offset)) {
    offset = ~~offset;
    if (isFinite(length)) {
      length = ~~length;
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
    offset = ~~length;
    length = swap;
  }

  var remaining = this.length - offset;
  if (util.isUndefined(length) || length > remaining)
    length = remaining;

  encoding = !!encoding ? (encoding + '').toLowerCase() : 'utf8';

  if (string.length > 0 && (length < 0 || offset < 0))
    throw new RangeError('attempt to write beyond buffer bounds');

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

  var buf = new Buffer();
  sliceOnto(this, buf, start, end);
  buf.length = end - start;
  if (buf.length > 0)
    buf.parent = util.isUndefined(this.parent) ? this : this.parent;

  return buf;
};


function checkOffset(offset, ext, length) {
  if (offset < 0 || offset + ext > length)
    throw new RangeError('index out of range');
}


Buffer.prototype.readUInt8 = function(offset, noAssert) {
  offset = ~~offset;
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
  offset = ~~offset;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  return readUInt16(this, offset, false, noAssert);
};


Buffer.prototype.readUInt16BE = function(offset, noAssert) {
  offset = ~~offset;
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
  offset = ~~offset;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return readUInt32(this, offset, false);
};


Buffer.prototype.readUInt32BE = function(offset, noAssert) {
  offset = ~~offset;
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
  offset = ~~offset;
  if (!noAssert)
    checkOffset(offset, 1, this.length);
  if (!(this[offset] & 0x80))
    return (this[offset]);
  return ((0xff - this[offset] + 1) * -1);
};


function readInt16(buffer, offset, isBigEndian) {
  var val = readUInt16(buffer, offset, isBigEndian);
  if (!(val & 0x8000))
    return val;
  return (0xffff - val + 1) * -1;
}


Buffer.prototype.readInt16LE = function(offset, noAssert) {
  offset = ~~offset;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  return readInt16(this, offset, false);
};


Buffer.prototype.readInt16BE = function(offset, noAssert) {
  offset = ~~offset;
  if (!noAssert)
    checkOffset(offset, 2, this.length);
  return readInt16(this, offset, true);
};


function readInt32(buffer, offset, isBigEndian) {
  var val = readUInt32(buffer, offset, isBigEndian);
  if (!(val & 0x80000000))
    return (val);
  return (0xffffffff - val + 1) * -1;
}


Buffer.prototype.readInt32LE = function(offset, noAssert) {
  offset = ~~offset;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return readInt32(this, offset, false);
};


Buffer.prototype.readInt32BE = function(offset, noAssert) {
  offset = ~~offset;
  if (!noAssert)
    checkOffset(offset, 4, this.length);
  return readInt32(this, offset, true);
};


function checkInt(buffer, value, offset, ext, max, min) {
  if (value > max || value < min)
    throw new TypeError('value is out of bounds');
  if (offset < 0 || offset + ext > buffer.length || buffer.length + offset < 0)
    throw new RangeError('index out of range');
}


Buffer.prototype.writeUInt8 = function(value, offset, noAssert) {
  value = +value;
  offset = ~~offset;
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
  offset = ~~offset;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0xffff, 0);
  return writeUInt16(this, value, offset, false);
};


Buffer.prototype.writeUInt16BE = function(value, offset, noAssert) {
  value = +value;
  offset = ~~offset;
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
  offset = ~~offset;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0xffffffff, 0);
  return writeUInt32(this, value, offset, false);
};


Buffer.prototype.writeUInt32BE = function(value, offset, noAssert) {
  value = +value;
  offset = ~~offset;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0xffffffff, 0);
  return writeUInt32(this, value, offset, true);
};


/*
 * We now move onto our friends in the signed number category. Unlike unsigned
 * numbers, we're going to have to worry a bit more about how we put values into
 * arrays. Since we are only worrying about signed 32-bit values, we're in
 * slightly better shape. Unfortunately, we really can't do our favorite binary
 * & in this system. It really seems to do the wrong thing. For example:
 *
 * > -32 & 0xff
 * 224
 *
 * What's happening above is really: 0xe0 & 0xff = 0xe0. However, the results of
 * this aren't treated as a signed number. Ultimately a bad thing.
 *
 * What we're going to want to do is basically create the unsigned equivalent of
 * our representation and pass that off to the wuint* functions. To do that
 * we're going to do the following:
 *
 *  - if the value is positive
 *      we can pass it directly off to the equivalent wuint
 *  - if the value is negative
 *      we do the following computation:
 *         mb + val + 1, where
 *         mb   is the maximum unsigned value in that byte size
 *         val  is the Javascript negative integer
 *
 *
 * As a concrete value, take -128. In signed 16 bits this would be 0xff80. If
 * you do out the computations:
 *
 * 0xffff - 128 + 1
 * 0xffff - 127
 * 0xff80
 *
 * You can then encode this value as the signed version. This is really rather
 * hacky, but it should work and get the job done which is our goal here.
 */

Buffer.prototype.writeInt8 = function(value, offset, noAssert) {
  value = +value;
  offset = ~~offset;
  if (!noAssert)
    checkInt(this, value, offset, 1, 0x7f, -0x80);
  if (value < 0) value = 0xff + value + 1;
  this[offset] = value;
  return offset + 1;
};


Buffer.prototype.writeInt16LE = function(value, offset, noAssert) {
  value = +value;
  offset = ~~offset;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0x7fff, -0x8000);
  if (value < 0) value = 0xffff + value + 1;
  return writeUInt16(this, value, offset, false);
};


Buffer.prototype.writeInt16BE = function(value, offset, noAssert) {
  value = +value;
  offset = ~~offset;
  if (!noAssert)
    checkInt(this, value, offset, 2, 0x7fff, -0x8000);
  if (value < 0) value = 0xffff + value + 1;
  return writeUInt16(this, value, offset, true);
};


Buffer.prototype.writeInt32LE = function(value, offset, noAssert) {
  value = +value;
  offset = ~~offset;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000);
  if (value < 0) value = 0xffffffff + value + 1;
  return writeUInt32(this, value, offset, false);
};


Buffer.prototype.writeInt32BE = function(value, offset, noAssert) {
  value = +value;
  offset = ~~offset;
  if (!noAssert)
    checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000);
  if (value < 0) value = 0xffffffff + value + 1;
  return writeUInt32(this, value, offset, true);
};
