'use strict';

if (!process.binding('config').hasIntl) {
  return;
}

const normalizeEncoding = require('internal/util').normalizeEncoding;
const Buffer = require('buffer').Buffer;

const icu = process.binding('icu');
const { isUint8Array } = process.binding('util');

// Transcodes the Buffer from one encoding to another, returning a new
// Buffer instance.
function transcode(source, fromEncoding, toEncoding) {
  if (!isUint8Array(source))
    throw new TypeError('"source" argument must be a Buffer or Uint8Array');
  if (source.length === 0) return Buffer.alloc(0);

  fromEncoding = normalizeEncoding(fromEncoding) || fromEncoding;
  toEncoding = normalizeEncoding(toEncoding) || toEncoding;
  const result = icu.transcode(source, fromEncoding, toEncoding);
  if (typeof result !== 'number')
    return result;

  const code = icu.icuErrName(result);
  const err = new Error(`Unable to transcode Buffer [${code}]`);
  err.code = code;
  err.errno = result;
  throw err;
}

module.exports = {
  transcode
};
