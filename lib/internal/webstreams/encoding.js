'use strict';

const {
  ObjectDefineProperties,
  String,
  StringPrototypeCharCodeAt,
  Symbol,
  Uint8Array,
} = primordials;

const {
  TextDecoder,
  TextEncoder,
} = require('internal/encoding');

const {
  TransformStream,
} = require('internal/webstreams/transformstream');

const { customInspect } = require('internal/webstreams/util');

const {
  codes: {
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  customInspectSymbol: kInspect,
  kEmptyObject,
  kEnumerableProperty,
} = require('internal/util');

const kHandle = Symbol('kHandle');
const kTransform = Symbol('kTransform');
const kType = Symbol('kType');
const kPendingHighSurrogate = Symbol('kPendingHighSurrogate');

/**
 * @typedef {import('./readablestream').ReadableStream} ReadableStream
 * @typedef {import('./writablestream').WritableStream} WritableStream
 */

function isTextEncoderStream(value) {
  return typeof value?.[kHandle] === 'object' &&
         value?.[kType] === 'TextEncoderStream';
}

function isTextDecoderStream(value) {
  return typeof value?.[kHandle] === 'object' &&
         value?.[kType] === 'TextDecoderStream';
}

class TextEncoderStream {
  constructor() {
    this[kPendingHighSurrogate] = null;
    this[kType] = 'TextEncoderStream';
    this[kHandle] = new TextEncoder();
    this[kTransform] = new TransformStream({
      transform: (chunk, controller) => {
        // https://encoding.spec.whatwg.org/#encode-and-enqueue-a-chunk
        chunk = String(chunk);
        let finalChunk = '';
        for (let i = 0; i < chunk.length; i++) {
          const item = chunk[i];
          const codeUnit = StringPrototypeCharCodeAt(item, 0);
          if (this[kPendingHighSurrogate] !== null) {
            const highSurrogate = this[kPendingHighSurrogate];
            this[kPendingHighSurrogate] = null;
            if (0xDC00 <= codeUnit && codeUnit <= 0xDFFF) {
              finalChunk += highSurrogate + item;
              continue;
            }
            finalChunk += '\uFFFD';
          }
          if (0xD800 <= codeUnit && codeUnit <= 0xDBFF) {
            this[kPendingHighSurrogate] = item;
            continue;
          }
          if (0xDC00 <= codeUnit && codeUnit <= 0xDFFF) {
            finalChunk += '\uFFFD';
            continue;
          }
          finalChunk += item;
        }
        if (finalChunk) {
          const value = this[kHandle].encode(finalChunk);
          controller.enqueue(value);
        }
      },
      flush: (controller) => {
        // https://encoding.spec.whatwg.org/#encode-and-flush
        if (this[kPendingHighSurrogate] !== null) {
          controller.enqueue(new Uint8Array([0xEF, 0xBF, 0xBD]));
        }
      },
    });
  }

  /**
   * @readonly
   * @type {string}
   */
  get encoding() {
    if (!isTextEncoderStream(this))
      throw new ERR_INVALID_THIS('TextEncoderStream');
    return this[kHandle].encoding;
  }

  /**
   * @readonly
   * @type {ReadableStream}
   */
  get readable() {
    if (!isTextEncoderStream(this))
      throw new ERR_INVALID_THIS('TextEncoderStream');
    return this[kTransform].readable;
  }

  /**
   * @readonly
   * @type {WritableStream}
   */
  get writable() {
    if (!isTextEncoderStream(this))
      throw new ERR_INVALID_THIS('TextEncoderStream');
    return this[kTransform].writable;
  }

  [kInspect](depth, options) {
    if (!isTextEncoderStream(this))
      throw new ERR_INVALID_THIS('TextEncoderStream');
    return customInspect(depth, options, 'TextEncoderStream', {
      encoding: this[kHandle].encoding,
      readable: this[kTransform].readable,
      writable: this[kTransform].writable,
    });
  }
}

class TextDecoderStream {
  /**
   * @param {string} [encoding]
   * @param {{
   *   fatal? : boolean,
   *   ignoreBOM? : boolean,
   * }} [options]
   */
  constructor(encoding = 'utf-8', options = kEmptyObject) {
    this[kType] = 'TextDecoderStream';
    this[kHandle] = new TextDecoder(encoding, options);
    this[kTransform] = new TransformStream({
      transform: (chunk, controller) => {
        const value = this[kHandle].decode(chunk, { stream: true });
        if (value)
          controller.enqueue(value);
      },
      flush: (controller) => {
        const value = this[kHandle].decode();
        if (value)
          controller.enqueue(value);
        controller.terminate();
      },
    });
  }

  /**
   * @readonly
   * @type {string}
   */
  get encoding() {
    if (!isTextDecoderStream(this))
      throw new ERR_INVALID_THIS('TextDecoderStream');
    return this[kHandle].encoding;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get fatal() {
    if (!isTextDecoderStream(this))
      throw new ERR_INVALID_THIS('TextDecoderStream');
    return this[kHandle].fatal;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get ignoreBOM() {
    if (!isTextDecoderStream(this))
      throw new ERR_INVALID_THIS('TextDecoderStream');
    return this[kHandle].ignoreBOM;
  }

  /**
   * @readonly
   * @type {ReadableStream}
   */
  get readable() {
    if (!isTextDecoderStream(this))
      throw new ERR_INVALID_THIS('TextDecoderStream');
    return this[kTransform].readable;
  }

  /**
   * @readonly
   * @type {WritableStream}
   */
  get writable() {
    if (!isTextDecoderStream(this))
      throw new ERR_INVALID_THIS('TextDecoderStream');
    return this[kTransform].writable;
  }

  [kInspect](depth, options) {
    if (!isTextDecoderStream(this))
      throw new ERR_INVALID_THIS('TextDecoderStream');
    return customInspect(depth, options, 'TextDecoderStream', {
      encoding: this[kHandle].encoding,
      fatal: this[kHandle].fatal,
      ignoreBOM: this[kHandle].ignoreBOM,
      readable: this[kTransform].readable,
      writable: this[kTransform].writable,
    });
  }
}

ObjectDefineProperties(TextEncoderStream.prototype, {
  encoding: kEnumerableProperty,
  readable: kEnumerableProperty,
  writable: kEnumerableProperty,
});

ObjectDefineProperties(TextDecoderStream.prototype, {
  encoding: kEnumerableProperty,
  fatal: kEnumerableProperty,
  ignoreBOM: kEnumerableProperty,
  readable: kEnumerableProperty,
  writable: kEnumerableProperty,
});

module.exports = {
  TextEncoderStream,
  TextDecoderStream,
};
