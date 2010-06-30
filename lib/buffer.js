var Buffer = process.binding('buffer').Buffer;

exports.Buffer = Buffer;

function toHex (n) {
  if (n < 16) return "0" + n.toString(16);
  return n.toString(16);
}

Buffer.prototype.inspect = function () {
  var s = "<Buffer ";
  for (var i = 0; i < this.length; i++) {
    s += toHex(this[i]);
    if (i != this.length - 1) s += ' ';
  }
  s += ">";
  return s;
};

Buffer.prototype.toString = function (encoding, start, stop) {
  encoding = (encoding || 'utf8').toLowerCase();
  if (!start) start = 0;
  if (!stop) stop = this.length;

  switch (encoding) {
    case 'utf8':
    case 'utf-8':
      return this.utf8Slice(start, stop);

    case 'ascii':
      return this.asciiSlice(start, stop);

    case 'binary':
      return this.binarySlice(start, stop);

    default:
      throw new Error('Unknown encoding');
  }
};

Buffer.prototype.write = function (string) {
  // Support both (string, offset, encoding)
  // and the legacy (string, encoding, offset)
  var offset, encoding;

  if (typeof arguments[1] == 'string') {
    encoding = arguments[1];
    offset = arguments[2];
  } else {
    offset = arguments[1];
    encoding = arguments[2];
  }

  offset = offset || 0;
  encoding = encoding || 'utf8';

  switch (encoding) {
    case 'utf8':
    case 'utf-8':
      return this.utf8Write(string, offset);

    case 'ascii':
      return this.asciiWrite(string, offset);

    case 'binary':
      return this.binaryWrite(string, offset);

    default:
      throw new Error('Unknown encoding');
  }
};






