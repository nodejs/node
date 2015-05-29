'use strict';

const Buffer = require('buffer').Buffer;

function assertEncoding(encoding) {
  // Do not cache `Buffer.isEncoding`, some modules monkey-patch it to support
  // additional encodings
  if (encoding && !Buffer.isEncoding(encoding)) {
    throw new Error('Unknown encoding: ' + encoding);
  }
}

// StringDecoder provides an interface for efficiently splitting a series of
// buffers into a series of JS strings without breaking apart multi-byte
// characters. CESU-8 is handled as part of the UTF-8 encoding.
//
// @TODO Handling all encodings inside a single object makes it very difficult
// to reason about this code, so it should be split up in the future.
// @TODO There should be a utf8-strict encoding that rejects invalid UTF-8 code
// points as used by CESU-8.
const StringDecoder = exports.StringDecoder = function(encoding) {
  this.encoding = (encoding || 'utf8').toLowerCase().replace(/[-_]/, '');
  assertEncoding(encoding);
  switch (this.encoding) {
    case 'utf8':
      // CESU-8 represents each of Surrogate Pair by 3-bytes
      this.surrogateSize = 3;
      break;
    case 'ucs2':
    case 'utf16le':
      // UTF-16 represents each of Surrogate Pair by 2-bytes
      this.surrogateSize = 2;
      this.detectIncompleteChar = utf16DetectIncompleteChar;
      break;
    case 'base64':
      // Base-64 stores 3 bytes in 4 chars, and pads the remainder.
      this.surrogateSize = 3;
      this.detectIncompleteChar = base64DetectIncompleteChar;
      break;
    default:
      this.write = passThroughWrite;
      return;
  }

  // Enough space to store all bytes of a single character. UTF-8 needs 4
  // bytes, but CESU-8 may require up to 6 (3 bytes per surrogate).
  this.charBuffer = new Buffer(6);
  // Number of bytes received for the current incomplete multi-byte character.
  this.charReceived = 0;
  // Number of bytes expected for the current incomplete multi-byte character.
  this.charLength = 0;
};


// write decodes the given buffer and returns it as JS string that is
// guaranteed to not contain any partial multi-byte characters. Any partial
// character found at the end of the buffer is buffered up, and will be
// returned when calling write again with the remaining bytes.
//
// Note: Converting a Buffer containing an orphan surrogate to a String
// currently works, but converting a String to a Buffer (via `new Buffer`, or
// Buffer#write) will replace incomplete surrogates with the unicode
// replacement character. See https://codereview.chromium.org/121173009/ .
StringDecoder.prototype.write = function(buffer) {
  var charStr = '';
  var buflen = buffer.length;
  var charBuffer = this.charBuffer;
  var charLength = this.charLength;
  var charReceived = this.charReceived;
  var surrogateSize = this.surrogateSize;
  var encoding = this.encoding;
  // if our last write ended with an incomplete multibyte character
  while (charLength) {
    // determine how many remaining bytes this buffer has to offer for this char
    var diff = charLength - charReceived;
    var available = (buflen >= diff) ? diff : buflen;

    // add the new bytes to the char buffer
    buffer.copy(charBuffer, charReceived, 0, available);
    charReceived += available;

    if (charReceived < charLength) {
      // still not enough chars in this buffer? wait for more ...

      this.charLength = charLength;
      this.charReceived = charReceived;

      return '';
    }

    // remove bytes belonging to the current character from the buffer
    buffer = buffer.slice(available, buflen);
    buflen = buffer.length;

    // get the character that was split
    charStr = charBuffer.toString(encoding, 0, charLength);

    // CESU-8: lead surrogate (D800-DBFF) is also the incomplete character
    var charCode = charStr.charCodeAt(charStr.length - 1);
    if (charCode >= 0xD800 && charCode <= 0xDBFF) {
      charLength += surrogateSize;
      charStr = '';
      continue;
    }
    charReceived = charLength = 0;

    // if there are no more bytes in this buffer, just emit our char
    if (buflen === 0) {
      this.charLength = charLength;
      this.charReceived = charReceived;

      return charStr;
    }
  }

  // determine and set charLength / charReceived
  if (this.detectIncompleteChar(buffer))
    charLength = this.charLength;
  charReceived = this.charReceived;

  var end = buflen;
  if (charLength) {
    // buffer the incomplete character bytes we got
    buffer.copy(charBuffer, 0, buflen - charReceived, end);
    end -= charReceived;
  }

  this.charLength = charLength;
  charStr += buffer.toString(encoding, 0, end);

  var end = charStr.length - 1;
  var charCode = charStr.charCodeAt(end);
  // CESU-8: lead surrogate (D800-DBFF) is also the incomplete character
  if (charCode >= 0xD800 && charCode <= 0xDBFF) {
    charLength += surrogateSize;
    charReceived += surrogateSize;
    charBuffer.copy(charBuffer, surrogateSize, 0, surrogateSize);
    buffer.copy(charBuffer, 0, 0, surrogateSize);

    this.charLength = charLength;
    this.charReceived = charReceived;

    return charStr.substring(0, end);
  }

  // or just emit the charStr
  return charStr;
};

// detectIncompleteChar determines if there is an incomplete UTF-8 character at
// the end of the given buffer. If so, it sets this.charLength to the byte
// length that character, and sets this.charReceived to the number of bytes
// that are available for this character.
StringDecoder.prototype.detectIncompleteChar = function(buffer) {
  var buflen = buffer.length;
  // determine how many bytes we have to check at the end of this buffer
  var i = (buflen >= 3) ? 3 : buflen;
  var newlen = false;

  // Figure out if one of the last i bytes of our buffer announces an
  // incomplete char.
  for (; i > 0; i--) {
    var c = buffer[buflen - i];

    // See http://en.wikipedia.org/wiki/UTF-8#Description

    // 110XXXXX
    if (i === 1 && c >> 5 === 0x06) {
      this.charLength = 2;
      newlen = true;
      break;
    }

    // 1110XXXX
    if (i <= 2 && c >> 4 === 0x0E) {
      this.charLength = 3;
      newlen = true;
      break;
    }

    // 11110XXX
    if (i <= 3 && c >> 3 === 0x1E) {
      this.charLength = 4;
      newlen = true;
      break;
    }
  }

  this.charReceived = i;

  return newlen;
};

StringDecoder.prototype.end = function(buffer) {
  var res = '';
  if (buffer && buffer.length)
    res = this.write(buffer);

  var charReceived = this.charReceived;
  if (charReceived) {
    var cr = charReceived;
    var buf = this.charBuffer;
    var enc = this.encoding;
    res += buf.toString(enc, 0, cr);
  }

  return res;
};

function passThroughWrite(buffer) {
  return buffer.toString(this.encoding);
}

function utf16DetectIncompleteChar(buffer) {
  var charReceived = this.charReceived = buffer.length % 2;
  this.charLength = charReceived ? 2 : 0;
  return true;
}

function base64DetectIncompleteChar(buffer) {
  var charReceived = this.charReceived = buffer.length % 3;
  this.charLength = charReceived ? 3 : 0;
  return true;
}
