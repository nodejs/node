'use strict';

if (!process.binding('config').hasIntl) {
  return;
}

const normalizeEncoding = require('internal/util').normalizeEncoding;
const Buffer = require('buffer').Buffer;

const icu = process.binding('icu');

// Transcodes the Buffer from one encoding to another, returning a new
// Buffer instance.
exports.transcode = function transcode(source, fromEncoding, toEncoding) {
  if (!Buffer.isBuffer(source))
    throw new TypeError('"source" argument must be a Buffer');
  if (source.length === 0) return Buffer.alloc(0);

  fromEncoding = normalizeEncoding(fromEncoding) || fromEncoding;
  toEncoding = normalizeEncoding(toEncoding) || toEncoding;
  const result = icu.transcode(source, fromEncoding, toEncoding);
  if (Buffer.isBuffer(result))
    return result;

  const code = icu.icuErrName(result);
  const err = new Error(`Unable to transcode Buffer [${code}]`);
  err.code = code;
  err.errno = result;
  throw err;
};
