'use strict';

const Buffer = require('buffer').Buffer;

// Do not cache `Buffer.isEncoding` when checking encoding names as some
// modules monkey-patch it to support additional encodings
function normalizeEncoding(enc) {
  if (!enc) return 'utf8';
  var low;
  for (;;) {
    switch (enc) {
      case 'utf8':
      case 'utf-8':
        return 'utf8';
      case 'ucs2':
      case 'utf16le':
      case 'ucs-2':
      case 'utf-16le':
        return 'utf16le';
      case 'base64':
      case 'ascii':
      case 'latin1':
      case 'binary':
      case 'hex':
        return enc;
      default:
        if (low) {
          if (!Buffer.isEncoding(enc))
            throw new Error('Unknown encoding: ' + enc);
          return enc;
        }
        low = true;
        enc = ('' + enc).toLowerCase();
    }
  }
}

// StringDecoder provides an interface for efficiently splitting a series of
// buffers into a series of JS strings without breaking apart multi-byte
// characters.
exports.StringDecoder = StringDecoder;
function StringDecoder(encoding) {
  this.encoding = normalizeEncoding(encoding);
  var nb;
  switch (this.encoding) {
    case 'utf16le':
      this.text = utf16Text;
      this.end = utf16End;
      // fall through
    case 'utf8':
      nb = 4;
      break;
    case 'base64':
      this.text = base64Text;
      this.end = base64End;
      nb = 3;
      break;
    default:
      this.write = simpleWrite;
      this.end = simpleEnd;
      return;
  }
  this.lastNeed = 0;
  this.lastTotal = 0;
  this.lastChar = Buffer.allocUnsafe(nb);
}

StringDecoder.prototype.write = function(buf) {
  if (buf.length === 0)
    return '';
  var r;
  var i;
  if (this.lastNeed) {
    r = this.fillLast(buf);
    if (r === undefined)
      return '';
    i = this.lastNeed;
    this.lastNeed = 0;
  } else {
    i = 0;
  }
  if (i < buf.length)
    return (r ? r + this.text(buf, i) : this.text(buf, i));
  return r || '';
};

StringDecoder.prototype.end = utf8End;

// Returns only complete characters in a Buffer
StringDecoder.prototype.text = utf8Text;

// Attempts to complete a partial character using bytes from a Buffer
StringDecoder.prototype.fillLast = function(buf) {
  if (this.lastNeed <= buf.length) {
    buf.copy(this.lastChar, this.lastTotal - this.lastNeed, 0, this.lastNeed);
    return this.lastChar.toString(this.encoding, 0, this.lastTotal);
  }
  buf.copy(this.lastChar, this.lastTotal - this.lastNeed, 0, buf.length);
  this.lastNeed -= buf.length;
};

// Checks the type of a UTF-8 byte, whether it's ASCII, a leading byte, or a
// continuation byte.
function utf8CheckByte(byte) {
  if (byte <= 0x7F)
    return 0;
  else if (byte >> 5 === 0x06)
    return 2;
  else if (byte >> 4 === 0x0E)
    return 3;
  else if (byte >> 3 === 0x1E)
    return 4;
  return -1;
}

// Checks at most the last 3 bytes of a Buffer for an incomplete UTF-8
// character, returning the total number of bytes needed to complete the partial
// character (if applicable).
function utf8CheckIncomplete(self, buf, i) {
  var j = buf.length - 1;
  if (j < i)
    return 0;
  var nb = utf8CheckByte(buf[j--]);
  if (nb >= 0) {
    if (nb > 0)
      self.lastNeed = nb + 1 - (buf.length - j);
    return nb;
  }
  if (j < i)
    return 0;
  nb = utf8CheckByte(buf[j--]);
  if (nb >= 0) {
    if (nb > 0)
      self.lastNeed = nb + 1 - (buf.length - j);
    return nb;
  }
  if (j < i)
    return 0;
  nb = utf8CheckByte(buf[j--]);
  if (nb >= 0) {
    if (nb > 0)
      self.lastNeed = nb + 1 - (buf.length - j);
    return nb;
  }
  return 0;
}

// Returns all complete UTF-8 characters in a Buffer. If the Buffer ended on a
// partial character, the character's bytes are buffered until the required
// number of bytes are available.
function utf8Text(buf, i) {
  const total = utf8CheckIncomplete(this, buf, i);
  if (!this.lastNeed)
    return buf.toString('utf8', i);
  this.lastTotal = total;
  const end = buf.length - (total - this.lastNeed);
  buf.copy(this.lastChar, 0, end);
  return buf.toString('utf8', i, end);
}

// For UTF-8, a replacement character for each buffered byte of a (partial)
// character needs to be added to the output.
function utf8End(buf) {
  const r = (buf && buf.length ? this.write(buf) : '');
  if (this.lastNeed)
    return r + '\ufffd'.repeat(this.lastTotal - this.lastNeed);
  return r;
}

// UTF-16LE typically needs two bytes per character, but even if we have an even
// number of bytes available, we need to check if we end on a leading/high
// surrogate. In that case, we need to wait for the next two bytes in order to
// decode the last character properly.
function utf16Text(buf, i) {
  if ((buf.length - i) % 2 === 0) {
    const r = buf.toString('utf16le', i);
    if (r) {
      const c = r.charCodeAt(r.length - 1);
      if (c >= 0xD800 && c <= 0xDBFF) {
        this.lastNeed = 2;
        this.lastTotal = 4;
        this.lastChar[0] = buf[buf.length - 2];
        this.lastChar[1] = buf[buf.length - 1];
        return r.slice(0, -1);
      }
    }
    return r;
  }
  this.lastNeed = 1;
  this.lastTotal = 2;
  this.lastChar[0] = buf[buf.length - 1];
  return buf.toString('utf16le', i, buf.length - 1);
}

// For UTF-16LE we do not explicitly append special replacement characters if we
// end on a partial character, we simply let v8 handle that.
function utf16End(buf) {
  const r = (buf && buf.length ? this.write(buf) : '');
  if (this.lastNeed) {
    const end = this.lastTotal - this.lastNeed;
    return r + this.lastChar.toString('utf16le', 0, end);
  }
  return r;
}

function base64Text(buf, i) {
  const n = (buf.length - i) % 3;
  if (n === 0)
    return buf.toString('base64', i);
  this.lastNeed = 3 - n;
  this.lastTotal = 3;
  if (n === 1) {
    this.lastChar[0] = buf[buf.length - 1];
  } else {
    this.lastChar[0] = buf[buf.length - 2];
    this.lastChar[1] = buf[buf.length - 1];
  }
  return buf.toString('base64', i, buf.length - n);
}


function base64End(buf) {
  const r = (buf && buf.length ? this.write(buf) : '');
  if (this.lastNeed)
    return r + this.lastChar.toString('base64', 0, 3 - this.lastNeed);
  return r;
}

// Pass bytes on through for single-byte encodings (e.g. ascii, latin1, hex)
function simpleWrite(buf) {
  return buf.toString(this.encoding);
}

function simpleEnd(buf) {
  return (buf && buf.length ? this.write(buf) : '');
}
