var Buffer = process.binding('buffer').Buffer;

exports.Buffer = Buffer;

function toHex (n) {
  if (n < 16) return "0" + n.toString(16);
  return n.toString(16);
}

Buffer.isBuffer = function (b) {
  return b instanceof Buffer;
};

Buffer.prototype.inspect = function () {
  var out = [],
      len = this.length;
  for (var i = 0; i < len; i++) {
    out[i] = toHex(this[i]);
  }
  return "<Buffer " + out.join(" ") + ">";
};

Buffer.prototype.toString = function (encoding, start, stop) {
  encoding = String(encoding || 'utf8').toLowerCase();
  start = +start || 0;
  if (typeof stop == "undefined") stop = this.length;

  // Fastpath empty strings
  if (+stop == start) {
    return '';
  }

  switch (encoding) {
    case 'utf8':
    case 'utf-8':
      return this.utf8Slice(start, stop);

    case 'ascii':
      return this.asciiSlice(start, stop);

    case 'binary':
      return this.binarySlice(start, stop);

    case 'base64':
      return this.base64Slice(start, stop);

    default:
      throw new Error('Unknown encoding');
  }
};

Buffer.prototype.write = function (string, offset, encoding) {
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

Buffer.prototype.get = function (index) {
  return this[index];
};

Buffer.prototype.set = function (index, value) {
  return this[index] = value;
};
