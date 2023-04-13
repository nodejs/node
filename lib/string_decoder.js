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
  TypedArrayPrototypeSubarray,
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
  encodings,
} = internalBinding('string_decoder');
const internalUtil = require('internal/util');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_UNKNOWN_ENCODING,
} = require('internal/errors').codes;
const isEncoding = Buffer[internalUtil.kIsEncodingSymbol];

const encodingsMap = {};
for (let i = 0; i < encodings.length; ++i)
  encodingsMap[encodings[i]] = i;

class StringDecoder {
  #nativeDecoder = Buffer.alloc(kSize);

  /**
   * StringDecoder provides an interface for efficiently splitting a series of
   * buffers into a series of JS strings without breaking apart multi-byte
   * characters.
   *
   * @param {string} [encoding=utf-8]
   */
  constructor(encoding) {
    this.encoding = this.#normalizeEncoding(encoding);
    this.#nativeDecoder[kEncodingField] = encodingsMap[this.encoding];
  }

  // Do not cache `Buffer.isEncoding` when checking encoding names as some
  // modules monkey-patch it to support additional encodings
  /**
   * Normalize encoding notation
   *
   * @param {string} enc
   * @returns {"utf8" | "utf16le" | "hex" | "ascii"
   *           | "base64" | "latin1" | "base64url"}
   * @throws {TypeError} Throws an error when encoding is invalid
   */
  #normalizeEncoding(enc) {
    const nenc = internalUtil.normalizeEncoding(enc);
    if (nenc === undefined) {
      if (Buffer.isEncoding === isEncoding || !Buffer.isEncoding(enc))
        throw new ERR_UNKNOWN_ENCODING(enc);
      return enc;
    }
    return nenc;
  }

  /**
   * Returns a decoded string, omitting any incomplete multi-bytes
   * characters at the end of the Buffer, or TypedArray, or DataView
   *
   * @param {string | Buffer | TypedArray | DataView} buf
   * @returns {string}
   * @throws {TypeError} Throws when buf is not in one of supported types
   */
  write(buf) {
    if (typeof buf === 'string')
      return buf;
    if (!ArrayBufferIsView(buf))
      throw new ERR_INVALID_ARG_TYPE('buf',
                                     ['Buffer', 'TypedArray', 'DataView'],
                                     buf);
    return decode(this.#nativeDecoder, buf);
  }

  /**
   * Returns any remaining input stored in the internal buffer as a string.
   * After end() is called, the stringDecoder object can be reused for new
   * input.
   *
   * @param {string | Buffer | TypedArray | DataView} [buf]
   * @returns {string}
   */
  end(buf) {
    let ret = '';
    if (buf !== undefined)
      ret = this.write(buf);
    if (this.#nativeDecoder[kBufferedBytes] > 0)
      ret += flush(this.#nativeDecoder);
    return ret;
  }

  /* Everything below this line is undocumented legacy stuff. */
  /**
   *
   * @param {string | Buffer | TypedArray | DataView} buf
   * @param {number} offset
   * @returns {string}
   */
  text(buf, offset) {
    this.#nativeDecoder[kMissingBytes] = 0;
    this.#nativeDecoder[kBufferedBytes] = 0;
    return this.write(buf.slice(offset));
  }

  get lastChar() {
    return TypedArrayPrototypeSubarray(this.#nativeDecoder, kIncompleteCharactersStart, kIncompleteCharactersEnd);
  }

  get lastNeed() {
    return this.#nativeDecoder[kMissingBytes];
  }

  get lastTotal() {
    return this.#nativeDecoder[kBufferedBytes] + this.#nativeDecoder[kMissingBytes];
  }
}

ObjectDefineProperties(StringDecoder.prototype, {
  lastChar: internalUtil.kEnumerableProperty,
  lastNeed: internalUtil.kEnumerableProperty,
  lastTotal: internalUtil.kEnumerableProperty,
});

exports.StringDecoder = StringDecoder;
