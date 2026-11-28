'use strict';

// Reads batches of Uint8Array chunks from a Node.js Readable.

const {
  ArrayPrototypePush,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_STREAM_PREMATURE_CLOSE,
  },
} = require('internal/errors');

function toUint8Array(chunk) {
  if (typeof chunk === 'string') {
    chunk = require('buffer').Buffer.from(chunk);
  }
  return new Uint8Array(TypedArrayPrototypeGetBuffer(chunk),
                        TypedArrayPrototypeGetByteOffset(chunk),
                        TypedArrayPrototypeGetByteLength(chunk));
}

class NodeReadableReader {
  #stream;
  #attached = false;
  #ended = false;
  #error = null;
  #waiter = null;
  #onEnd;

  /**
   * @param {import('stream').Readable} stream
   * @param {() => void} [onEnd] Invoked once the stream has ended.
   */
  constructor(stream, onEnd) {
    this.#stream = stream;
    this.#onEnd = onEnd;
  }

  get ended() {
    return this.#ended;
  }

  #attach() {
    if (this.#attached) return;
    this.#attached = true;
    const stream = this.#stream;
    stream.on('readable', () => this.#wake());
    stream.on('end', () => {
      this.#ended = true;
      this.#onEnd?.();
      this.#wake();
    });
    stream.on('error', (err) => {
      this.#error ??= err;
      this.#wake();
    });
    stream.on('close', () => {
      if (!this.#ended) this.#error ??= new ERR_STREAM_PREMATURE_CLOSE();
      this.#wake();
    });
    if (stream.readableEnded) {
      this.#ended = true;
      this.#onEnd?.();
    }
  }

  #wake() {
    const waiter = this.#waiter;
    if (waiter === null) return;
    this.#waiter = null;
    waiter.resolve();
  }

  #drain() {
    const stream = this.#stream;
    let batch = null;
    let chunk;
    while ((chunk = stream.read()) !== null) {
      batch ??= [];
      ArrayPrototypePush(batch, toUint8Array(chunk));
    }
    return batch;
  }

  /**
   * @returns {Promise<Uint8Array[]|null>}
   */
  read() {
    this.#attach();
    const batch = this.#drain();
    if (batch !== null) return PromiseResolve(batch);
    if (this.#ended) return PromiseResolve(null);
    if (this.#error !== null) return PromiseReject(this.#error);
    return this.#readAsync();
  }

  async #readAsync() {
    for (;;) {
      const waiter = PromiseWithResolvers();
      this.#waiter = waiter;
      await waiter.promise;
      const batch = this.#drain();
      if (batch !== null) return batch;
      if (this.#ended) return null;
      if (this.#error !== null) throw this.#error;
    }
  }
}

module.exports = {
  NodeReadableReader,
  toUint8Array,
};
