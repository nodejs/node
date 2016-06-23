'use strict';

const Buffer = require('buffer').Buffer;
const internalUtil = require('internal/util');
const isEncoding = Buffer[internalUtil.kIsEncodingSymbol];

// Do not cache `Buffer.isEncoding` when checking encoding names as some
// modules monkey-patch it to support additional encodings
function normalizeEncoding(enc) {
  const nenc = internalUtil.normalizeEncoding(enc);
  if (typeof nenc !== 'string' &&
      (Buffer.isEncoding === isEncoding || !Buffer.isEncoding(enc)))
    throw new Error(`Unknown encoding: ${enc}`);
  return nenc || enc;
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
      this.complete = utf16Complete;
      this.flush = simpleFlush;
      // fall through
    case 'utf8':
      nb = 4;
      break;
    case 'base64':
      this.complete = base64Complete;
      this.flush = simpleFlush;
      nb = 3;
      break;
    default:
      this.write = simpleWrite;
      this.end = simpleEnd;
      return;
  }
  this.partial = 0;
  this.lastChar = Buffer.allocUnsafe(nb);
}

StringDecoder.prototype.write = function(buf) {
  if (buf.length === 0)
    return '';
  const partial = this.partial;
  if (!partial)
    return this.text(buf, 0, buf.length);

  // We have incomplete characters in partial many bytes from last run.
  // Copy bytes from buf to fill lastChar (if there is enough input).
  const newHeadLen = Math.min(buf.length, this.lastChar.length - partial);
  const totalHeadLen = newHeadLen + partial;
  buf.copy(this.lastChar, partial, 0, newHeadLen);
  // Now we have totalHeadLen bytes of input in lastChar, try to convert that.
  let r = this.text(this.lastChar, 0, totalHeadLen);
  if (this.partial <= newHeadLen) // consumed at least all the old head
    r += this.text(buf, newHeadLen - this.partial, buf.length);
  return r;
};

// Returns only complete characters in a Buffer
StringDecoder.prototype.text = function(buf, start, end) {
  if (start === end)
    return '';
  const complete = this.complete(buf, start, end);
  this.partial = end - complete;
  if (this.partial && buf !== this.lastChar)
    buf.copy(this.lastChar, 0, complete, end);
  if (start === complete)
    return '';
  return buf.toString(this.encoding, start, complete);
};

// Returns a suitable representation of incomplete characters as well
StringDecoder.prototype.end = function(buf) {
  let r = (buf && buf.length ? this.write(buf) : '');
  if (this.partial) {
    r += this.flush();
    this.partial = 0;
  }
  return r;
};

// Given (buf, start, end), determine the maximal n <= end such that
// buf.slice(start, n) contains only complete characters
StringDecoder.prototype.complete = utf8Complete;

// Returns a string representation of the this.partial bytes in
// this.lastChar which represent an incomplete character
StringDecoder.prototype.flush = utf8Flush;

// Checks at most the last 3 bytes of a Buffer for an incomplete UTF-8
// character, returning the position after the last complete character.
function utf8Complete(buf, start, end) {
  if (start > end - 3)
    start = end - 3;
  for (let i = end - 1; i >= start; --i) {
    const byte = buf[i];
    let numBytes;
    if (byte >> 6 === 0x02)
      continue; // continuation byte
    else if (byte >> 5 === 0x06)
      numBytes = 2;
    else if (byte >> 4 === 0x0E)
      numBytes = 3;
    else if (byte >> 3 === 0x1E)
      numBytes = 4;
    else
      numBytes = 1; // ASCII or invalid
    if (i + numBytes > end) // incomplete
      return i; // continue next run at leading byte
    // Have complete sequence, possibly followed by garbage continuation.
    return end;
  }
  // Ends in valid 4-byte sequence or invalid continuation characters.
  // Either way the input is complete, so convert it as is.
  return end;
}

// For UTF-8, a replacement character for each buffered byte of a (partial)
// character needs to be added to the output.
function utf8Flush() {
  return '\ufffd'.repeat(this.partial);
}

// UTF-16LE typically needs two bytes per character, but even if we have an even
// number of bytes available, we need to check if we end on a leading/high
// surrogate. In that case, we need to wait for the next two bytes in order to
// decode the last character properly.
function utf16Complete(buf, start, end) {
  if ((end - start) & 1)
    --end;
  if (end > start) {
    const byte = buf[end - 1];
    if (byte >= 0xD8 && byte <= 0xDB)
      return end - 2;
  }
  return end;
}

function base64Complete(buf, start, end) {
  return end - (end - start) % 3;
}

// For UTF-16LE and Base64 we do not explicitly append special replacement
// characters if we end on a partial character, we simply let v8 handle that.
function simpleFlush() {
  return this.lastChar.toString(this.encoding, 0, this.partial);
}

// Pass bytes on through for single-byte encodings (e.g. ascii, latin1, hex)
function simpleWrite(buf) {
  return buf.toString(this.encoding);
}

function simpleEnd(buf) {
  return (buf && buf.length ? this.write(buf) : '');
}
