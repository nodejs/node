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

var SlowBuffer = process.binding('buffer').SlowBuffer;
var assert = require('assert');


function toHex(n) {
  if (n < 16) return '0' + n.toString(16);
  return n.toString(16);
}


SlowBuffer.prototype.inspect = function() {
  var out = [],
      len = this.length;
  for (var i = 0; i < len; i++) {
    out[i] = toHex(this[i]);
  }
  return '<SlowBuffer ' + out.join(' ') + '>';
};


SlowBuffer.prototype.hexSlice = function(start, end) {
  var len = this.length;

  if (!start || start < 0) start = 0;
  if (!end || end < 0 || end > len) end = len;

  var out = '';
  for (var i = start; i < end; i++) {
    out += toHex(this[i]);
  }
  return out;
};



SlowBuffer.prototype.toString = function(encoding, start, end) {
  encoding = String(encoding || 'utf8').toLowerCase();
  start = +start || 0;
  if (typeof end == 'undefined') end = this.length;

  // Fastpath empty strings
  if (+end == start) {
    return '';
  }

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
      return this.ucs2Slice(start, end);

    default:
      throw new Error('Unknown encoding');
  }
};


SlowBuffer.prototype.hexWrite = function(string, offset, length) {
  offset = +offset || 0;
  var remaining = this.length - offset;
  if (!length) {
    length = remaining;
  } else {
    length = +length;
    if (length > remaining) {
      length = remaining;
    }
  }

  // must be an even number of digits
  var strLen = string.length;
  if (strLen % 2) {
    throw new Error('Invalid hex string');
  }
  if (length > strLen / 2) {
    length = strLen / 2;
  }
  for (var i = 0; i < length; i++) {
    var byte = parseInt(string.substr(i * 2, 2), 16);
    if (isNaN(byte)) throw new Error('Invalid hex string');
    this[offset + i] = byte;
  }
  return i;
};


SlowBuffer.prototype.write = function(string, offset, length, encoding) {
  // Support both (string, offset, length, encoding)
  // and the legacy (string, encoding, offset, length)
  if (isFinite(offset)) {
    if (!isFinite(length)) {
      encoding = length;
      length = undefined;
    }
  } else {  // legacy
    var swap = encoding;
    encoding = offset;
    offset = length;
    length = swap;
  }

  offset = +offset || 0;
  var remaining = this.length - offset;
  if (!length) {
    length = remaining;
  } else {
    length = +length;
    if (length > remaining) {
      length = remaining;
    }
  }
  encoding = String(encoding || 'utf8').toLowerCase();

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
      return this.base64Write(string, offset, length);

    case 'ucs2':
    case 'ucs-2':
      return this.ucs2Write(string, offset, length);

    default:
      throw new Error('Unknown encoding');
  }
};


// slice(start, end)
SlowBuffer.prototype.slice = function(start, end) {
  if (end === undefined) end = this.length;

  if (end > this.length) {
    throw new Error('oob');
  }
  if (start > end) {
    throw new Error('oob');
  }

  return new Buffer(this, end - start, +start);
};


// Buffer

function Buffer(subject, encoding, offset) {
  if (!(this instanceof Buffer)) {
    return new Buffer(subject, encoding, offset);
  }

  var type;

  // Are we slicing?
  if (typeof offset === 'number') {
    this.length = encoding;
    this.parent = subject;
    this.offset = offset;
  } else {
    // Find the length
    switch (type = typeof subject) {
      case 'number':
        this.length = subject;
        break;

      case 'string':
        this.length = Buffer.byteLength(subject, encoding);
        break;

      case 'object': // Assume object is an array
        this.length = subject.length;
        break;

      default:
        throw new Error('First argument needs to be a number, ' +
                        'array or string.');
    }

    if (this.length > Buffer.poolSize) {
      // Big buffer, just alloc one.
      this.parent = new SlowBuffer(this.length);
      this.offset = 0;

    } else {
      // Small buffer.
      if (!pool || pool.length - pool.used < this.length) allocPool();
      this.parent = pool;
      this.offset = pool.used;
      pool.used += this.length;
    }

    // Treat array-ish objects as a byte array.
    if (isArrayIsh(subject)) {
      for (var i = 0; i < this.length; i++) {
        this.parent[i + this.offset] = subject[i];
      }
    } else if (type == 'string') {
      // We are a string
      this.length = this.write(subject, 0, encoding);
    }
  }

  SlowBuffer.makeFastBuffer(this.parent, this, this.offset, this.length);
}

function isArrayIsh(subject) {
  return Array.isArray(subject) || Buffer.isBuffer(subject) ||
         subject && typeof subject === 'object' &&
         typeof subject.length === 'number';
}

exports.SlowBuffer = SlowBuffer;
exports.Buffer = Buffer;

Buffer.poolSize = 8 * 1024;
var pool;

function allocPool() {
  pool = new SlowBuffer(Buffer.poolSize);
  pool.used = 0;
}


// Static methods
Buffer.isBuffer = function isBuffer(b) {
  return b instanceof Buffer || b instanceof SlowBuffer;
};


// Inspect
Buffer.prototype.inspect = function inspect() {
  var out = [],
      len = this.length;
  for (var i = 0; i < len; i++) {
    out[i] = toHex(this.parent[i + this.offset]);
  }
  return '<Buffer ' + out.join(' ') + '>';
};


Buffer.prototype.get = function get(i) {
  if (i < 0 || i >= this.length) throw new Error('oob');
  return this.parent[this.offset + i];
};


Buffer.prototype.set = function set(i, v) {
  if (i < 0 || i >= this.length) throw new Error('oob');
  return this.parent[this.offset + i] = v;
};


// write(string, offset = 0, length = buffer.length-offset, encoding = 'utf8')
Buffer.prototype.write = function(string, offset, length, encoding) {
  // Support both (string, offset, length, encoding)
  // and the legacy (string, encoding, offset, length)
  if (isFinite(offset)) {
    if (!isFinite(length)) {
      encoding = length;
      length = undefined;
    }
  } else {  // legacy
    var swap = encoding;
    encoding = offset;
    offset = length;
    length = swap;
  }

  offset = +offset || 0;
  var remaining = this.length - offset;
  if (!length) {
    length = remaining;
  } else {
    length = +length;
    if (length > remaining) {
      length = remaining;
    }
  }
  encoding = String(encoding || 'utf8').toLowerCase();

  var ret;
  switch (encoding) {
    case 'hex':
      ret = this.parent.hexWrite(string, this.offset + offset, length);
      break;

    case 'utf8':
    case 'utf-8':
      ret = this.parent.utf8Write(string, this.offset + offset, length);
      break;

    case 'ascii':
      ret = this.parent.asciiWrite(string, this.offset + offset, length);
      break;

    case 'binary':
      ret = this.parent.binaryWrite(string, this.offset + offset, length);
      break;

    case 'base64':
      // Warning: maxLength not taken into account in base64Write
      ret = this.parent.base64Write(string, this.offset + offset, length);
      break;

    case 'ucs2':
    case 'ucs-2':
      ret = this.parent.ucs2Write(string, this.offset + offset, length);
      break;

    default:
      throw new Error('Unknown encoding');
  }

  Buffer._charsWritten = SlowBuffer._charsWritten;

  return ret;
};


// toString(encoding, start=0, end=buffer.length)
Buffer.prototype.toString = function(encoding, start, end) {
  encoding = String(encoding || 'utf8').toLowerCase();

  if (typeof start == 'undefined' || start < 0) {
    start = 0;
  } else if (start > this.length) {
    start = this.length;
  }

  if (typeof end == 'undefined' || end > this.length) {
    end = this.length;
  } else if (end < 0) {
    end = 0;
  }

  start = start + this.offset;
  end = end + this.offset;

  switch (encoding) {
    case 'hex':
      return this.parent.hexSlice(start, end);

    case 'utf8':
    case 'utf-8':
      return this.parent.utf8Slice(start, end);

    case 'ascii':
      return this.parent.asciiSlice(start, end);

    case 'binary':
      return this.parent.binarySlice(start, end);

    case 'base64':
      return this.parent.base64Slice(start, end);

    case 'ucs2':
    case 'ucs-2':
      return this.parent.ucs2Slice(start, end);

    default:
      throw new Error('Unknown encoding');
  }
};


// byteLength
Buffer.byteLength = SlowBuffer.byteLength;


// fill(value, start=0, end=buffer.length)
Buffer.prototype.fill = function fill (value, start, end) {
  value || (value = 0);
  start || (start = 0);
  end   || (end   = this.length);

  if (typeof value === "string") {
    value = value.charCodeAt(0);
  }
  if (!(typeof value === "number") || isNaN(value)) {
    throw new Error("value is not a number");
  }

  if (end < start) throw new Error("end < start");

  // Fill 0 bytes; we're done
  if (end === start) return 0;
  if (this.length == 0) return 0;

  if (start < 0 || start >= this.length) {
    throw new Error("start out of bounds");
  }

  if (end < 0 || end > this.length) {
    throw new Error("end out of bounds");
  }

  return this.parent.fill(value,
                          start + this.offset,
                          end + this.offset);
};


// copy(targetBuffer, targetStart=0, sourceStart=0, sourceEnd=buffer.length)
Buffer.prototype.copy = function(target, target_start, start, end) {
  var source = this;
  start || (start = 0);
  end || (end = this.length);
  target_start || (target_start = 0);

  if (end < start) throw new Error('sourceEnd < sourceStart');

  // Copy 0 bytes; we're done
  if (end === start) return 0;
  if (target.length == 0 || source.length == 0) return 0;

  if (target_start < 0 || target_start >= target.length) {
    throw new Error('targetStart out of bounds');
  }

  if (start < 0 || start >= source.length) {
    throw new Error('sourceStart out of bounds');
  }

  if (end < 0 || end > source.length) {
    throw new Error('sourceEnd out of bounds');
  }

  // Are we oob?
  if (end > this.length) {
    end = this.length;
  }

  if (target.length - target_start < end - start) {
    end = target.length - target_start + start;
  }

  return this.parent.copy(target.parent,
                          target_start + target.offset,
                          start + this.offset,
                          end + this.offset);
};


// slice(start, end)
Buffer.prototype.slice = function(start, end) {
  if (end === undefined) end = this.length;
  if (end > this.length) throw new Error('oob');
  if (start > end) throw new Error('oob');

  return new Buffer(this.parent, end - start, +start + this.offset);
};


// Legacy methods for backwards compatibility.

Buffer.prototype.utf8Slice = function(start, end) {
  return this.toString('utf8', start, end);
};

Buffer.prototype.binarySlice = function(start, end) {
  return this.toString('binary', start, end);
};

Buffer.prototype.asciiSlice = function(start, end) {
  return this.toString('ascii', start, end);
};

Buffer.prototype.utf8Write = function(string, offset) {
  return this.write(string, offset, 'utf8');
};

Buffer.prototype.binaryWrite = function(string, offset) {
  return this.write(string, offset, 'binary');
};

Buffer.prototype.asciiWrite = function(string, offset) {
  return this.write(string, offset, 'ascii');
};

Buffer.prototype.readUInt8 = function(offset, endian) {
  var buffer = this;

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset < buffer.length,
    'Trying to read beyond buffer length');

  return buffer[offset];
};


Buffer.prototype.readUInt16 = function(offset, endian) {
  var val = 0;
  var buffer = this;

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 1 < buffer.length,
    'Trying to read beyond buffer length');

  if (endian == 'big') {
    val = buffer[offset] << 8;
    val |= buffer[offset + 1];
  } else {
    val = buffer[offset];
    val |= buffer[offset + 1] << 8;
  }

  return val;
};


Buffer.prototype.readUInt32 = function(offset, endian) {
  var val = 0;
  var buffer = this;

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 3 < buffer.length,
    'Trying to read beyond buffer length');

  if (endian == 'big') {
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
Buffer.prototype.readInt8 = function(offset, endian) {
  var buffer = this;
  var neg;

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset < buffer.length,
    'Trying to read beyond buffer length');

  neg = buffer[offset] & 0x80;
  if (!neg) {
    return (buffer[offset]);
  }

  return ((0xff - buffer[offset] + 1) * -1);
};


Buffer.prototype.readInt16 = function(offset, endian) {
  var buffer = this;
  var neg;

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 1 < buffer.length,
    'Trying to read beyond buffer length');

  val = buffer.readUInt16(offset, endian);
  neg = val & 0x8000;
  if (!neg) {
    return val;
  }

  return (0xffff - val + 1) * -1;
};


Buffer.prototype.readInt32 = function(offset, endian) {
  var buffer = this;
  var neg;

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 3 < buffer.length,
    'Trying to read beyond buffer length');

  val = buffer.readUInt32(offset, endian);
  neg = val & 0x80000000;
  if (!neg) {
    return (val);
  }

  return (0xffffffff - val + 1) * -1;
};


Buffer.prototype.readFloat = function(offset, endian) {
  var buffer = this;

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 3 < buffer.length,
    'Trying to read beyond buffer length');

  return require('buffer_ieee754').readIEEE754(buffer, offset, endian, 23, 4);
};

Buffer.prototype.readDouble = function(offset, endian) {
  var buffer = this;

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 7 < buffer.length,
    'Trying to read beyond buffer length');

  return require('buffer_ieee754').readIEEE754(buffer, offset, endian, 52, 8);
};


/*
 * We have to make sure that the value is a valid integer. This means that it is
 * non-negative. It has no fractional component and that it does not exceed the
 * maximum allowed value.
 *
 *      value           The number to check for validity
 *
 *      max             The maximum value
 */
function verifuint(value, max) {
  assert.ok(typeof (value) == 'number',
    'cannot write a non-number as a number');

  assert.ok(value >= 0,
    'specified a negative value for writing an unsigned value');

  assert.ok(value <= max, 'value is larger than maximum value for type');

  assert.ok(Math.floor(value) === value, 'value has a fractional component');
}


Buffer.prototype.writeUInt8 = function(value, offset, endian) {
  var buffer = this;

  assert.ok(value !== undefined && value !== null,
    'missing value');

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset < buffer.length,
    'trying to write beyond buffer length');

  verifuint(value, 0xff);
  buffer[offset] = value;
};


Buffer.prototype.writeUInt16 = function(value, offset, endian) {
  var buffer = this;

  assert.ok(value !== undefined && value !== null,
    'missing value');

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 1 < buffer.length,
    'trying to write beyond buffer length');

  verifuint(value, 0xffff);

  if (endian == 'big') {
    buffer[offset] = (value & 0xff00) >>> 8;
    buffer[offset + 1] = value & 0x00ff;
  } else {
    buffer[offset + 1] = (value & 0xff00) >>> 8;
    buffer[offset] = value & 0x00ff;
  }
};


Buffer.prototype.writeUInt32 = function(value, offset, endian) {
  var buffer = this;

  assert.ok(value !== undefined && value !== null,
    'missing value');

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 3 < buffer.length,
    'trying to write beyond buffer length');

  verifuint(value, 0xffffffff);
  if (endian == 'big') {
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

/*
 * A series of checks to make sure we actually have a signed 32-bit number
 */
function verifsint(value, max, min) {
  assert.ok(typeof (value) == 'number',
    'cannot write a non-number as a number');

  assert.ok(value <= max, 'value larger than maximum allowed value');

  assert.ok(value >= min, 'value smaller than minimum allowed value');

  assert.ok(Math.floor(value) === value, 'value has a fractional component');
}


function verifIEEE754(value, max, min) {
  assert.ok(typeof (value) == 'number',
    'cannot write a non-number as a number');

  assert.ok(value <= max, 'value larger than maximum allowed value');

  assert.ok(value >= min, 'value smaller than minimum allowed value');
}


Buffer.prototype.writeInt8 = function(value, offset, endian) {
  var buffer = this;

  assert.ok(value !== undefined && value !== null,
    'missing value');

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset < buffer.length,
    'Trying to write beyond buffer length');

  verifsint(value, 0x7f, -0xf0);

  if (value >= 0) {
    buffer.writeUInt8(value, offset, endian);
  } else {
    buffer.writeUInt8(0xff + value + 1, offset, endian);
  }
};


Buffer.prototype.writeInt16 = function(value, offset, endian) {
  var buffer = this;

  assert.ok(value !== undefined && value !== null,
    'missing value');

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 1 < buffer.length,
    'Trying to write beyond buffer length');

  verifsint(value, 0x7fff, -0xf000);

  if (value >= 0) {
    buffer.writeUInt16(value, offset, endian);
  } else {
    buffer.writeUInt16(0xffff + value + 1, offset, endian);
  }
};


Buffer.prototype.writeInt32 = function(value, offset, endian) {
  var buffer = this;

  assert.ok(value !== undefined && value !== null,
    'missing value');

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 3 < buffer.length,
    'Trying to write beyond buffer length');

  verifsint(value, 0x7fffffff, -0xf0000000);
  if (value >= 0) {
    buffer.writeUInt32(value, offset, endian);
  } else {
    buffer.writeUInt32(0xffffffff + value + 1, offset, endian);
  }
};


Buffer.prototype.writeFloat = function(value, offset, endian) {
  var buffer = this;

  assert.ok(value !== undefined && value !== null,
    'missing value');

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 3 < buffer.length,
    'Trying to write beyond buffer length');

  verifIEEE754(value, 3.4028234663852886e+38, -3.4028234663852886e+38);
  require('buffer_ieee754').writeIEEE754(buffer, value, offset, endian, 23, 4);
};


Buffer.prototype.writeDouble = function(value, offset, endian) {
  var buffer = this;

  assert.ok(value !== undefined && value !== null,
    'missing value');

  assert.ok(endian !== undefined && endian !== null,
    'missing endian');

  assert.ok(endian == 'big' || endian == 'little',
    'bad endian value');

  assert.ok(offset !== undefined && offset !== null,
    'missing offset');

  assert.ok(offset + 7 < buffer.length,
    'Trying to write beyond buffer length');

  verifIEEE754(value, 1.7976931348623157E+308, -1.7976931348623157E+308);
  require('buffer_ieee754').writeIEEE754(buffer, value, offset, endian, 52, 8);
};

