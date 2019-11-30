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

'use strict';

const {
  ArrayBufferIsView,
  ObjectDefineProperties,
  Symbol,
} = primordials;

const { Buffer } = require('buffer');
const {
  kIncompleteCharactersStart,
  kIncompleteCharactersEnd,
  kMissingBytes,
  kBufferedBytes,
  kEncodingField,
  kSize,
  decode,
  flush,
  encodings
} = internalBinding('string_decoder');
const internalUtil = require('internal/util');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_UNKNOWN_ENCODING
} = require('internal/errors').codes;
const isEncoding = Buffer[internalUtil.kIsEncodingSymbol];

const kNativeDecoder = Symbol('kNativeDecoder');

// Do not cache `Buffer.isEncoding` when checking encoding names as some
// modules monkey-patch it to support additional encodings
function normalizeEncoding(enc) {
  const nenc = internalUtil.normalizeEncoding(enc);
  if (nenc === undefined) {
    if (Buffer.isEncoding === isEncoding || !Buffer.isEncoding(enc))
      throw new ERR_UNKNOWN_ENCODING(enc);
    return enc;
  }
  return nenc;
}

const encodingsMap = {};
for (let i = 0; i < encodings.length; ++i)
  encodingsMap[encodings[i]] = i;

// StringDecoder provides an interface for efficiently splitting a series of
// buffers into a series of JS strings without breaking apart multi-byte
// characters.
function StringDecoder(encoding) {
  this.encoding = normalizeEncoding(encoding);
  this[kNativeDecoder] = Buffer.alloc(kSize);
  this[kNativeDecoder][kEncodingField] = encodingsMap[this.encoding];
}

StringDecoder.prototype.write = function write(buf) {
  if (typeof buf === 'string')
    return buf;
  if (!ArrayBufferIsView(buf))
    throw new ERR_INVALID_ARG_TYPE('buf',
                                   ['Buffer', 'TypedArray', 'DataView'],
                                   buf);
  return decode(this[kNativeDecoder], buf);
};

StringDecoder.prototype.end = function end(buf) {
  let ret = '';
  if (buf !== undefined)
    ret = this.write(buf);
  if (this[kNativeDecoder][kBufferedBytes] > 0)
    ret += flush(this[kNativeDecoder]);
  return ret;
};

/* Everything below this line is undocumented legacy stuff. */
StringDecoder.prototype.text = function text(buf, offset) {
  this[kNativeDecoder][kMissingBytes] = 0;
  this[kNativeDecoder][kBufferedBytes] = 0;
  return this.write(buf.slice(offset));
};

ObjectDefineProperties(StringDecoder.prototype, {
  lastChar: {
    configurable: true,
    enumerable: true,
    get() {
      return this[kNativeDecoder].subarray(kIncompleteCharactersStart,
                                           kIncompleteCharactersEnd);
    }
  },
  lastNeed: {
    configurable: true,
    enumerable: true,
    get() {
      return this[kNativeDecoder][kMissingBytes];
    }
  },
  lastTotal: {
    configurable: true,
    enumerable: true,
    get() {
      return this[kNativeDecoder][kBufferedBytes] +
             this[kNativeDecoder][kMissingBytes];
    }
  }
});

exports.StringDecoder = StringDecoder;
