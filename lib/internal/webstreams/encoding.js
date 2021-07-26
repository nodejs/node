'use strict';

const {
  ObjectDefineProperties,
  Symbol,
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
  kEnumerableProperty,
} = require('internal/util');

const kHandle = Symbol('kHandle');
const kTransform = Symbol('kTransform');
const kType = Symbol('kType');

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
    this[kType] = 'TextEncoderStream';
    this[kHandle] = new TextEncoder();
    this[kTransform] = new TransformStream({
      transform: (chunk, controller) => {
        const value = this[kHandle].encode(chunk);
        if (value)
          controller.enqueue(value);
      },
      flush: (controller) => {
        const value = this[kHandle].encode();
        if (value.byteLength > 0)
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
  constructor(encoding = 'utf-8', options = {}) {
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
