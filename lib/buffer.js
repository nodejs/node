var SlowBuffer = process.binding('buffer').Buffer;


function toHex (n) {
  if (n < 16) return "0" + n.toString(16);
  return n.toString(16);
}


SlowBuffer.prototype.inspect = function () {
  var out = [],
      len = this.length;
  for (var i = 0; i < len; i++) {
    out[i] = toHex(this[i]);
  }
  return "<SlowBuffer " + out.join(" ") + ">";
};


SlowBuffer.prototype.toString = function (encoding, start, end) {
  encoding = String(encoding || 'utf8').toLowerCase();
  start = +start || 0;
  if (typeof end == "undefined") end = this.length;

  // Fastpath empty strings
  if (+end == start) {
    return '';
  }

  switch (encoding) {
    case 'utf8':
    case 'utf-8':
      return this.utf8Slice(start, end);

    case 'ascii':
      return this.asciiSlice(start, end);

    case 'binary':
      return this.binarySlice(start, end);

    case 'base64':
      return this.base64Slice(start, end);

    default:
      throw new Error('Unknown encoding');
  }
};


SlowBuffer.prototype.write = function (string, offset, encoding) {
  // Support both (string, offset, encoding)
  // and the legacy (string, encoding, offset)
  if (!isFinite(offset)) {
    var swap = encoding;
    encoding = offset;
    offset = swap;
  }

  offset = +offset || 0;
  encoding = String(encoding || 'utf8').toLowerCase();

  switch (encoding) {
    case 'utf8':
    case 'utf-8':
      return this.utf8Write(string, offset);

    case 'ascii':
      return this.asciiWrite(string, offset);

    case 'binary':
      return this.binaryWrite(string, offset);

    case 'base64':
      return this.base64Write(string, offset);

    default:
      throw new Error('Unknown encoding');
  }
};


// Buffer
var POOLSIZE = 8*1024;
var pool;


function allocPool () {
  pool      = new SlowBuffer(POOLSIZE);
  pool.used = 0;
}


function Buffer (subject, encoding, legacy, slice_legacy) {
  if (!(this instanceof Buffer)) {
    return new Buffer(subject, encoding, legacy, slice_legacy);
  }

  var length, type;

  // Are we slicing?
  if (typeof legacy === 'number') {
    this.parent = subject;
    this.length = encoding;
    this.offset = legacy;
    legacy      = slice_legacy;
  } else {
    // Find the length
    switch (type = typeof subject) {
      case 'number':
        length = subject;
        break;

      case 'string':
        length = Buffer.byteLength(subject);
        break;

      case 'object': // Assume object is an array
        length = subject.length;
        break;

      default:
        throw new Error("First argument need to be an number, array or string.");
    }

    this.length = length;

    if (length > POOLSIZE) {
      // Big buffer, just alloc one.
      this.parent = new SlowBuffer(subject, encoding);
      this.offset = 0;
    } else {
      // Small buffer.
      if (!pool || pool.length - pool.used < length) allocPool();
      this.parent =  pool;
      this.offset =  pool.used;
      pool.used   += length;

      // Do we need to write stuff?
      if (type !== 'number') {
        // Assume object is an array
        if (type === 'object') {
          for (var i = 0; i < length; i++) {
            this.parent[i + this.offset] = subject[i];
          }
        } else {
          // We are a string
          this.write(subject, 0, encoding);
        }
      }
    }
  }

  // Make sure the api is equivilent to old buffers, unless user doesn't
  // want overhead
  if (legacy !== false) {
    SlowBuffer.makeFastBuffer(this.parent, this, this.offset, this.length);
  }
}

exports.Buffer = Buffer;


// Static methods
Buffer.isBuffer = function isBuffer(b) {
  return b instanceof Buffer;
};


// Inspect
Buffer.prototype.inspect = function inspect() {
  var out = [],
      len = this.length;
  for (var i = 0; i < len; i++) {
    out[i] = toHex(this.parent[i + this.offset]);
  }
  return "<Buffer " + out.join(" ") + ">";
};


Buffer.prototype.get = function get (i) {
  if (i < 0 || i >= this.length) throw new Error("oob");
  return this.parent[this.offset + i];
};


Buffer.prototype.set = function set (i, v) {
  if (i < 0 || i >= this.length) throw new Error("oob");
  return this.parent[this.offset + i] = v;
};


// write(string, offset = 0, encoding = 'utf8')
Buffer.prototype.write = function write (string, offset, encoding) {
  if (!isFinite(offset)) {
    var swap = encoding;
    encoding = offset;
    offset = swap;
  }

  offset   || (offset   = 0);
  encoding || (encoding = 'utf8');

  // Make sure we are not going to overflow
  var max_length = this.length - offset;
  if (Buffer.byteLength(string) > max_length) {
    // FIXME: Char length !== byte length
    string = string.slice(0, max_length);
  }

  return this.parent.write(string, this.offset + offset, encoding);
};


// toString(encoding, start=0, end=buffer.length)
Buffer.prototype.toString = function (encoding, start, end) {
  if (typeof encoding == 'undefined') encoding = 'utf8';

  if (typeof start == 'undefined' || start < 0) {
    start = 0;
  } else if (start > this.length) {
    start = this.length;
  }

  if (typeof end == "undefined" || end > this.length) {
    end = this.length;
  } else if (end < 0) {
    end = 0;
  }

  return this.parent.toString(encoding, start + this.offset, end + this.offset);
};


// byteLength
Buffer.byteLength = SlowBuffer.byteLength;


// copy(targetBuffer, targetStart, sourceStart, sourceEnd=buffer.length)
Buffer.prototype.copy = function copy (target, target_start, start, end) {
  var source = this;
  start || (start = 0);
  end   || (end   = this.length);

  if (end < start) throw new Error("sourceEnd < sourceStart");

  // Copy 0 bytes; we're done
  if (end === start) return 0;
  if (target.length == 0 || source.length == 0) return 0;

  if (target_start < 0 || target_start >= target.length) {
    throw new Error("targetStart out of bounds");
  }

  if (start < 0 || start >= source.length) {
    throw new Error("sourceStart out of bounds");
  }

  if (end < 0 || end > source.length) {
    throw new Error("sourceEnd out of bounds");
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
Buffer.prototype.slice = function (start, end, legacy) {
  if (end > this.length) {
    throw new Error("oob");
  }
  if (start > end) {
    throw new Error("oob");
  }

  return new Buffer(this.parent, end - start, +start + this.offset, legacy);
};

