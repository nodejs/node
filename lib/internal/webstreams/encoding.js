'use strict';

const {
  ObjectDefineProperties,
  String,
  StringPrototypeCharCodeAt,
  StringPrototypeSlice,
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
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  customInspectSymbol: kInspect,
  kEmptyObject,
  kEnumerableProperty,
} = require('internal/util');

/**
 * @typedef {import('./readablestream').ReadableStream} ReadableStream
 * @typedef {import('./writablestream').WritableStream} WritableStream
 */

class TextEncoderStream {
  #pendingHighSurrogate = null;
  #handle;
  #transform;

  constructor() {
    this.#handle = new TextEncoder();
    this.#transform = new TransformStream({
      transform: (chunk, controller) => {
        // https://encoding.spec.whatwg.org/#encode-and-enqueue-a-chunk
        //
        // The spec describes a per-code-unit loop whose only observable
        // effects are (a) holding back a high surrogate that ends a chunk
        // so it can be paired with a low surrogate starting the next chunk
        // and (b) replacing unpaired surrogates with U+FFFD. The encoder
        // already performs (b), so only the chunk boundaries need special
        // handling here.
        chunk = String(chunk);
        if (chunk.length === 0) {
          return;
        }
        if (this.#pendingHighSurrogate !== null) {
          chunk = this.#pendingHighSurrogate + chunk;
          this.#pendingHighSurrogate = null;
        }
        const lastCodeUnit = StringPrototypeCharCodeAt(chunk, chunk.length - 1);
        if (0xD800 <= lastCodeUnit && lastCodeUnit <= 0xDBFF) {
          // A high surrogate at the end of the chunk may pair with a low
          // surrogate at the start of the next one: hold it back.
          this.#pendingHighSurrogate = StringPrototypeSlice(chunk, -1);
          chunk = StringPrototypeSlice(chunk, 0, -1);
        }
        if (chunk) {
          const value = this.#handle.encode(chunk);
          controller.enqueue(value);
        }
      },
      flush: (controller) => {
        // https://encoding.spec.whatwg.org/#encode-and-flush
        if (this.#pendingHighSurrogate !== null) {
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
    return this.#handle.encoding;
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
    if (this == null)
      throw new ERR_INVALID_THIS('TextEncoderStream');
    return customInspect(depth, options, 'TextEncoderStream', {
      encoding: this.#handle.encoding,
      readable: this.#transform.readable,
      writable: this.#transform.writable,
    });
  }
}

class TextDecoderStream {
  #handle;
  #transform;

  /**
   * @param {string} [encoding]
   * @param {{
   *   fatal? : boolean,
   *   ignoreBOM? : boolean,
   * }} [options]
   */
  constructor(encoding = 'utf-8', options = kEmptyObject) {
    this.#handle = new TextDecoder(encoding, options);
    this.#transform = new TransformStream({
      transform: (chunk, controller) => {
        if (chunk === undefined) {
          throw new ERR_INVALID_ARG_TYPE('chunk', 'string', chunk);
        }
        const value = this.#handle.decode(chunk, { stream: true });
        if (value)
          controller.enqueue(value);
      },
      flush: (controller) => {
        const value = this.#handle.decode();
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
    return this.#handle.encoding;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get fatal() {
    return this.#handle.fatal;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get ignoreBOM() {
    return this.#handle.ignoreBOM;
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
    if (this == null)
      throw new ERR_INVALID_THIS('TextDecoderStream');
    return customInspect(depth, options, 'TextDecoderStream', {
      encoding: this.#handle.encoding,
      fatal: this.#handle.fatal,
      ignoreBOM: this.#handle.ignoreBOM,
      readable: this.#transform.readable,
      writable: this.#transform.writable,
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
