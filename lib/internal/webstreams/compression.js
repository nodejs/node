'use strict';

const {
  ObjectDefineProperties,
} = primordials;

const {
  codes: { ERR_INVALID_ARG_VALUE },
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

/**
 * @typedef {import('./readablestream').ReadableStream} ReadableStream
 * @typedef {import('./writablestream').WritableStream} WritableStream
 */

class CompressionStream {
  #handle;
  #transform;

  /**
   * @param {'deflate'|'gzip'} format
   */
  constructor(format) {
    switch (format) {
      case 'deflate':
        this.#handle = lazyZlib().createDeflate();
        break;
      case 'gzip':
        this.#handle = lazyZlib().createGzip();
        break;
      default:
        throw new ERR_INVALID_ARG_VALUE('format', format);
    }
    this.#transform = newReadableWritablePairFromDuplex(this.#handle);
  }

  /**
   * @readonly
   * @type {ReadableStream}
   */
  get readable() {
    return this.#transform.readable;
  }

  /**
   * @readonly
   * @type {WritableStream}
   */
  get writable() {
    return this.#transform.writable;
  }

  [kInspect](depth, options) {
    customInspect(depth, options, 'CompressionStream', {
      readable: this.#transform.readable,
      writable: this.#transform.writable,
    });
  }
}

class DecompressionStream {
  #handle;
  #transform;

  /**
   * @param {'deflate'|'gzip'} format
   */
  constructor(format) {
    switch (format) {
      case 'deflate':
        this.#handle = lazyZlib().createInflate();
        break;
      case 'gzip':
        this.#handle = lazyZlib().createGunzip();
        break;
      default:
        throw new ERR_INVALID_ARG_VALUE('format', format);
    }
    this.#transform = newReadableWritablePairFromDuplex(this.#handle);
  }

  /**
   * @readonly
   * @type {ReadableStream}
   */
  get readable() {
    return this.#transform.readable;
  }

  /**
   * @readonly
   * @type {WritableStream}
   */
  get writable() {
    return this.#transform.writable;
  }

  [kInspect](depth, options) {
    customInspect(depth, options, 'DecompressionStream', {
      readable: this.#transform.readable,
      writable: this.#transform.writable,
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
