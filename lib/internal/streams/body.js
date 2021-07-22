'use strict';

const { Buffer, Blob } = require('buffer');
const Duplex = require('internal/streams/duplex');
const Readable = require('internal/streams/readable');
const Writable = require('internal/streams/writable');
const duplexify = require('internal/streams/duplexify');
const { createDeferredPromise } = require('internal/util');
const { destroyer } = require('internal/streams/destroy');
const from = require('internal/streams/from');
const assert = require('internal/assert');

const {
  isBlob
} = require('internal/blob');

const {
  isBrandCheck,
} = require('internal/webstreams/util');

const isReadableStream =
  isBrandCheck('ReadableStream');
const isWritableStream =
  isBrandCheck('WritableStream');

const {
  isIterable,
  isDuplexNodeStream,
  isReadableNodeStream,
  isWritableNodeStream,
} = require('internal/streams/utils');

const {
  JSONParse,
  PromiseResolve,
  Symbol,
  SymbolAsyncIterator,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_RETURN_VALUE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

const kState = Symbol('kState');

// This is needed for pre node 17.
class BodyDuplex extends Duplex {
  constructor(options) {
    super(options);

    // https://github.com/nodejs/node/pull/34385

    if (options?.readable === false) {
      this._readableState.readable = false;
      this._readableState.ended = true;
      this._readableState.endEmitted = true;
    }

    if (options?.writable === false) {
      this._writableState.writable = false;
      this._writableState.ending = true;
      this._writableState.ended = true;
      this._writableState.finished = true;
    }
  }
}

class Body {
  constructor(body, options) {
    // TODO (ronag): What about TransformStream?

    if (body[kState]) {
      this[kState] = body[kState];
    } else if (
      isReadableStream(body?.readable) &&
      isWritableStream(body?.writable)
    ) {
      // TODO (ronag): Optimize. Delay conversion.
      const d = Duplex.fromWeb(body, options);
      this[kState] = { readable: d, writable: d };
    } else if (isWritableStream(body?.writable)) {
      // TODO (ronag): Optimize. Delay conversion.
      this[kState] = {
        readable: undefined,
        writable: Writable.fromWeb(body, options)
      };
    } else if (isReadableStream(body?.readable)) {
      // TODO (ronag): Optimize. Delay conversion.
      this[kState] = {
        readable: Readable.fromWeb(body, options),
        writable: undefined
      };
    } else if (isDuplexNodeStream(body)) {
      this[kState] = { readable: body, writable: body };
    } else if (isReadableNodeStream(body)) {
      this[kState] = { readable: body, writable: undefined };
    } else if (isWritableNodeStream(body)) {
      this[kState] = { readable: undefined, writable: body };
    } else if (isReadableStream(body)) {
      // TODO (ronag): Optimize. Delay conversion.
      this[kState] = {
        readable: Readable.fromWeb(body, options),
        writable: undefined
      };
    } else if (isWritableStream(body)) {
      // TODO (ronag): Optimize. Delay conversion.
      this[kState] = {
        readable: undefined,
        writable: Writable.fromWeb(body, options)
      };
    } else if (typeof body === 'function') {
      // TODO (ronag): Optimize. Delay conversion.
      assert(body.length > 0);

      const { value, write, final } = fromAsyncGen(body);

      if (isIterable(value)) {
        const d = from(BodyDuplex, value, {
          objectMode: true,
          highWaterMark: 1,
          ...options,
          write,
          final
        });

        this[kState] = { readable: d, writable: d };
      } else if (typeof value?.then === 'function') {
        let d;

        const promise = PromiseResolve(value)
          .then((val) => {
            if (val != null) {
              throw new ERR_INVALID_RETURN_VALUE('nully', 'body', val);
            }
          })
          .catch((err) => {
            destroyer(d, err);
          });

        d = new BodyDuplex({
          objectMode: true,
          highWaterMark: 1,
          ...options,
          readable: false,
          write,
          final(cb) {
            final(() => promise.then(cb, cb));
          }
        });

        this[kState] = { readable: d, writable: d };
      } else {
        throw new ERR_INVALID_RETURN_VALUE(
          'Iterable, AsyncIterable or AsyncFunction', 'body', value);
      }
    } else if (isBlob(body)) {
      // TODO (ronag): Optimize. Delay conversion.
      const d = new Body(async function* () {
        yield await body.arrayBuffer();
      }()).nodeStream();

      this[kState] = { readable: d, writable: d };
    } else if (isIterable(body)) {
      // TODO (ronag): Optimize. Delay conversion.
      const d = from(BodyDuplex, body, {
        objectMode: true,
        highWaterMark: 1,
        ...options,
        writable: false
      });

      this[kState] = { readable: d, writable: d };
    } else if (
      typeof body?.writable === 'object' ||
      typeof body?.readable === 'object'
    ) {
      // TODO (ronag): Optimize. Delay conversion.
      const readable = body?.readable ?
        isReadableNodeStream(body?.readable) ? body?.readable :
          new Body(body.readable).readableNodeStream() : undefined;
      const writable = body?.writable ?
        isWritableNodeStream(body?.writable) ? body?.writable :
          new Body(body.writable).writableNodeStream() : undefined;

      this[kState] = { readable, writable };
    } else {
      throw new ERR_INVALID_ARG_TYPE(
        'stream',
        ['Blob', 'ReadableStream', 'WritableStream', 'Stream', 'Iterable',
         'AsyncIterable', 'Function', '{ readable, writable } pair'],
        body)
      ;
    }
  }

  readableNodeStream() {
    const { readable } = this[kState];

    if (readable === null) {
      throw new ERR_INVALID_STATE('read lock');
    }

    this[kState].readable = null;

    // TODO (ronag): Hide Writable interface.
    return readable ?? new BodyDuplex({ readable: false, writable: false });
  }

  writableNodeStream() {
    const { writable } = this[kState];

    if (writable === null) {
      throw new ERR_INVALID_STATE('write lock');
    }

    this[kState].writable = null;

    // TODO (ronag): Hide Readable interface.
    return writable ?? new BodyDuplex({ readable: false, writable: false });
  }

  nodeStream() {
    if (this.readable === null) {
      throw new ERR_INVALID_STATE('read lock');
    }

    if (this.writable === null) {
      throw new ERR_INVALID_STATE('write lock');
    }

    if (this[kState].readable === this[kState].writable) {
      const d = this[kState].readable;
      this[kState].readable = null;
      this[kState].writable = null;
      return d;
    }

    return duplexify({
      readable: this.readableNodeStream(),
      writable: this.writableNodeStream(),
    });
  }

  readableWebStream() {
    return this.readableWebStream().asWeb();
  }

  writableWebStream() {
    return this.writableNodeStream().asWeb();
  }

  [SymbolAsyncIterator]() {
    return this.readableNodeStream()[SymbolAsyncIterator]();
  }

  async blob() {
    const sources = [];
    for await (const chunk of this.readableNodeStream()) {
      sources.push(chunk);
    }
    return new Blob(sources);
  }

  async buffer() {
    const sources = [];
    for await (const chunk of this.readableNodeStream()) {
      sources.push(chunk);
    }
    return Buffer.concat(sources);
  }

  async arrayBuffer() {
    const blob = await this.blob();
    return blob.arrayBuffer();
  }

  async text() {
    let ret = '';
    for await (const chunk of this.readableNodeStream()) {
      ret += chunk;
    }
    return ret;
  }

  async json() {
    return JSONParse(await this.text());
  }
}

function fromAsyncGen(fn) {
  let { promise, resolve } = createDeferredPromise();
  const value = fn(async function*() {
    while (true) {
      const { chunk, done, cb } = await promise;
      process.nextTick(cb);
      if (done) return;
      yield chunk;
      ({ promise, resolve } = createDeferredPromise());
    }
  }());

  return {
    value,
    write(chunk, encoding, cb) {
      resolve({ chunk, done: false, cb });
    },
    final(cb) {
      resolve({ done: true, cb });
    }
  };
}

module.exports = Body;
