'use strict';

const {
  ObjectDefineProperties,
  Symbol,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  newReadableWritablePairFromDuplex,
} = require('internal/webstreams/adapters');

const { customInspect } = require('internal/webstreams/util');

const {
  customInspectSymbol: kInspect,
  kEnumerableProperty,
} = require('internal/util');

let zlib;
function lazyZlib() {
  zlib ??= require('zlib');
  return zlib;
}

const kHandle = Symbol('kHandle');
const kTransform = Symbol('kTransform');
const kType = Symbol('kType');

/**
 * @typedef {import('./readablestream').ReadableStream} ReadableStream
 * @typedef {import('./writablestream').WritableStream} WritableStream
 */

function isCompressionStream(value) {
  return typeof value?.[kHandle] === 'object' &&
         value?.[kType] === 'CompressionStream';
}

function isDecompressionStream(value) {
  return typeof value?.[kHandle] === 'object' &&
         value?.[kType] === 'DecompressionStream';
}

class CompressionStream {
  /**
   * @param {'deflate'|'gzip'} format
   */
  constructor(format) {
    this[kType] = 'CompressionStream';
    switch (format) {
      case 'deflate':
        this[kHandle] = lazyZlib().createDeflate();
        break;
      case 'gzip':
        this[kHandle] = lazyZlib().createGzip();
        break;
      default:
        throw new ERR_INVALID_ARG_VALUE('format', format);
    }
    this[kTransform] = newReadableWritablePairFromDuplex(this[kHandle]);
  }

  /**
   * @readonly
   * @type {ReadableStream}
   */
  get readable() {
    if (!isCompressionStream(this))
      throw new ERR_INVALID_THIS('CompressionStream');
    return this[kTransform].readable;
  }

  /**
   * @readonly
   * @type {WritableStream}
   */
  get writable() {
    if (!isCompressionStream(this))
      throw new ERR_INVALID_THIS('CompressionStream');
    return this[kTransform].writable;
  }

  [kInspect](depth, options) {
    if (!isCompressionStream(this))
      throw new ERR_INVALID_THIS('CompressionStream');
    customInspect(depth, options, 'CompressionStream', {
      readable: this[kTransform].readable,
      writable: this[kTransform].writable,
    });
  }
}

class DecompressionStream {
  /**
   * @param {'deflate'|'gzip'} format
   */
  constructor(format) {
    this[kType] = 'DecompressionStream';
    switch (format) {
      case 'deflate':
        this[kHandle] = lazyZlib().createInflate();
        break;
      case 'gzip':
        this[kHandle] = lazyZlib().createGunzip();
        break;
      default:
        throw new ERR_INVALID_ARG_VALUE('format', format);
    }
    this[kTransform] = newReadableWritablePairFromDuplex(this[kHandle]);
  }

  /**
   * @readonly
   * @type {ReadableStream}
   */
  get readable() {
    if (!isDecompressionStream(this))
      throw new ERR_INVALID_THIS('DecompressionStream');
    return this[kTransform].readable;
  }

  /**
   * @readonly
   * @type {WritableStream}
   */
  get writable() {
    if (!isDecompressionStream(this))
      throw new ERR_INVALID_THIS('DecompressionStream');
    return this[kTransform].writable;
  }

  [kInspect](depth, options) {
    if (!isDecompressionStream(this))
      throw new ERR_INVALID_THIS('DecompressionStream');
    customInspect(depth, options, 'DecompressionStream', {
      readable: this[kTransform].readable,
      writable: this[kTransform].writable,
    });
  }
}

ObjectDefineProperties(CompressionStream.prototype, {
  readable: kEnumerableProperty,
  writable: kEnumerableProperty,
});

ObjectDefineProperties(DecompressionStream.prototype, {
  readable: kEnumerableProperty,
  writable: kEnumerableProperty,
});

module.exports = {
  CompressionStream,
  DecompressionStream,
};
