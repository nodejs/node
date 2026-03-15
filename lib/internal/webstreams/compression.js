'use strict';

const {
  ObjectDefineProperties,
  SymbolToStringTag,
} = primordials;

const {
  newReadableWritablePairFromDuplex,
  kValidateChunk,
  kDestroyOnSyncError,
} = require('internal/webstreams/adapters');

const { customInspect } = require('internal/webstreams/util');

const {
  isArrayBufferView,
  isSharedArrayBuffer,
} = require('internal/util/types');

const {
  customInspectSymbol: kInspect,
  kEnumerableProperty,
} = require('internal/util');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const { createEnumConverter } = require('internal/webidl');

let zlib;
function lazyZlib() {
  zlib ??= require('zlib');
  return zlib;
}

// Per the Compression Streams spec, chunks must be BufferSource
// (ArrayBuffer or ArrayBufferView not backed by SharedArrayBuffer).
function validateBufferSourceChunk(chunk) {
  if (isArrayBufferView(chunk) && isSharedArrayBuffer(chunk.buffer)) {
    throw new ERR_INVALID_ARG_TYPE(
      'chunk',
      ['ArrayBuffer', 'Buffer', 'TypedArray', 'DataView'],
      chunk,
    );
  }
}

const formatConverter = createEnumConverter('CompressionFormat', [
  'deflate',
  'deflate-raw',
  'gzip',
  'brotli',
]);

/**
 * @typedef {import('./readablestream').ReadableStream} ReadableStream
 * @typedef {import('./writablestream').WritableStream} WritableStream
 */

class CompressionStream {
  #handle;
  #transform;

  /**
   * @param {'deflate'|'deflate-raw'|'gzip'|'brotli'} format
   */
  constructor(format) {
    format = formatConverter(format, {
      prefix: "Failed to construct 'CompressionStream'",
      context: '1st argument',
    });
    switch (format) {
      case 'deflate':
        this.#handle = lazyZlib().createDeflate();
        break;
      case 'deflate-raw':
        this.#handle = lazyZlib().createDeflateRaw();
        break;
      case 'gzip':
        this.#handle = lazyZlib().createGzip();
        break;
      case 'brotli':
        this.#handle = lazyZlib().createBrotliCompress();
        break;
    }
    this.#transform = newReadableWritablePairFromDuplex(this.#handle, {
      [kValidateChunk]: validateBufferSourceChunk,
      [kDestroyOnSyncError]: true,
    });
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
    return customInspect(depth, options, 'CompressionStream', {
      readable: this.#transform.readable,
      writable: this.#transform.writable,
    });
  }
}

class DecompressionStream {
  #handle;
  #transform;

  /**
   * @param {'deflate'|'deflate-raw'|'gzip'|'brotli'} format
   */
  constructor(format) {
    format = formatConverter(format, {
      prefix: "Failed to construct 'DecompressionStream'",
      context: '1st argument',
    });
    switch (format) {
      case 'deflate':
        this.#handle = lazyZlib().createInflate({
          rejectGarbageAfterEnd: true,
        });
        break;
      case 'deflate-raw':
        this.#handle = lazyZlib().createInflateRaw({
          rejectGarbageAfterEnd: true,
        });
        break;
      case 'gzip':
        this.#handle = lazyZlib().createGunzip({
          rejectGarbageAfterEnd: true,
        });
        break;
      case 'brotli':
        this.#handle = lazyZlib().createBrotliDecompress({
          rejectGarbageAfterEnd: true,
        });
        break;
    }
    this.#transform = newReadableWritablePairFromDuplex(this.#handle, {
      [kValidateChunk]: validateBufferSourceChunk,
      [kDestroyOnSyncError]: true,
    });
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
    return customInspect(depth, options, 'DecompressionStream', {
      readable: this.#transform.readable,
      writable: this.#transform.writable,
    });
  }
}

ObjectDefineProperties(CompressionStream.prototype, {
  readable: kEnumerableProperty,
  writable: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'CompressionStream',
  },
});

ObjectDefineProperties(DecompressionStream.prototype, {
  readable: kEnumerableProperty,
  writable: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'DecompressionStream',
  },
});

module.exports = {
  CompressionStream,
  DecompressionStream,
};
