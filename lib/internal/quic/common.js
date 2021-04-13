'use strict';

const {
  FunctionPrototypeCall,
  ObjectDefineProperties,
  PromisePrototypeThen,
  PromisePrototypeFinally,
  PromiseReject,
  PromiseResolve,
  ReflectConstruct,
  Symbol,
  SymbolIterator,
  SymbolAsyncIterator,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  Event,
} = require('internal/event_target');

const {
  ArrayBufferViewSource,
  BlobSource,
  StreamSource,
  StreamBaseSource,
} = internalBinding('quic');

const {
  createDeferredPromise,
  customInspectSymbol: kInspect,
} = require('internal/util');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
  isPromise,
  isGeneratorObject,
} = require('util/types');

const {
  Readable,
  Writable,
  pipeline,
} = require('stream');

const {
  Buffer,
} = require('buffer');

const {
  isBlob,
  kHandle: kBlobHandle,
} = require('internal/blob');

const {
  inspect,
  TextEncoder,
} = require('util');

const {
  isReadableStream,
} = require('internal/webstreams/readablestream');

const {
  WritableStream,
} = require('internal/webstreams/writablestream');

const {
  kEnumerableProperty,
} = require('internal/webstreams/util');

const {
  getPromiseDetails,
  kPending,
} = internalBinding('util');

const {
  isFileHandle,
  kHandle: kFileHandle,
} = require('internal/fs/promises');

const { owner_symbol } = internalBinding('symbols');

const {
  onStreamRead,
  kUpdateTimer,
} = require('internal/stream_base_commons');

const {
  kHandle,
} = require('internal/quic/config');

const kAddSession = Symbol('kAddSession');
const kRemoveSession = Symbol('kRemoveSession');
const kAddStream = Symbol('kAddStream');
const kBlocked = Symbol('kBlocked');
const kClientHello = Symbol('kClientHello');
const kClose = Symbol('kClose');
const kCreatedStream = Symbol('kCreatedStream');
const kData = Symbol('kData');
const kDestroy = Symbol('kDestroy');
const kHandshakeComplete = Symbol('kHandshakeComplete');
const kHeaders = Symbol('kHeaders');
const kOCSP = Symbol('kOCSP');
const kMaybeStreamEvent = Symbol('kMaybeStreamEvent');
const kRemoveStream = Symbol('kRemoveStream');
const kResetStream = Symbol('kResetStream');
const kRespondWith = Symbol('kRespondWith');
const kSessionTicket = Symbol('kSessionTicket');
const kSetSource = Symbol('kSetSource');
const kState = Symbol('kState');
const kTrailers = Symbol('kTrailers');
const kType = Symbol('kType');

class DatagramEvent extends Event {
  constructor() { throw new ERR_ILLEGAL_CONSTRUCTOR(); }

  get datagram() {
    if (!isDatagramEvent(this))
      throw new ERR_INVALID_THIS('DatagramEvent');
    return this[kData].buffer;
  }

  get early() {
    if (!isDatagramEvent(this))
      throw new ERR_INVALID_THIS('DatagramEvent');
    return this[kData].early;
  }

  get session() {
    if (!isDatagramEvent(this))
      throw new ERR_INVALID_THIS('DatagramEvent');
    return this[kData].session;
  }

  [kInspect](depth, options) {
    if (!isDatagramEvent(this))
      throw new ERR_INVALID_THIS('DatagramEvent');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `${this[kType]} ${inspect({
      datagram: this[kData].buffer,
      early: this[kData].early,
      session: this[kData].session,
    }, opts)}`;
  }
}

ObjectDefineProperties(DatagramEvent.prototype, {
  datagram: kEnumerableProperty,
  early: kEnumerableProperty,
  session: kEnumerableProperty,
});

function isDatagramEvent(value) {
  return typeof value?.[kData] === 'object' &&
         value?.[kType] === 'DatagramEvent';
}

function createDatagramEvent(buffer, early, session) {
  return ReflectConstruct(
    class extends Event {
      constructor() {
        super('datagram');
        this[kType] = 'DatagramEvent';
        this[kData] = {
          buffer,
          early,
          session,
        };
      }
    },
    [],
    DatagramEvent);
}

class SessionEvent extends Event {
  constructor() { throw new ERR_ILLEGAL_CONSTRUCTOR(); }

  get session() {
    if (!isSessionEvent(this))
      throw new ERR_INVALID_THIS('SessionEvent');
    return this[kData].session;
  }

  [kInspect](depth, options) {
    if (!isSessionEvent(this))
      throw new ERR_INVALID_THIS('SessionEvent');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `${this[kType]} ${inspect({
      session: this[kData].session,
    }, opts)}`;
  }
}

ObjectDefineProperties(DatagramEvent.prototype, {
  session: kEnumerableProperty,
});

function isSessionEvent(value) {
  return typeof value?.[kData] === 'object' &&
         value?.[kType] === 'SessionEvent';
}

function createSessionEvent(session) {
  return ReflectConstruct(
    class extends Event {
      constructor() {
        super('session');
        this[kType] = 'SessionEvent';
        this[kData] = {
          session,
        };
      }
    },
    [],
    SessionEvent);
}

class StreamEvent extends Event {
  constructor() { throw new ERR_ILLEGAL_CONSTRUCTOR(); }

  get stream() {
    if (!isStreamEvent(this))
      throw new ERR_INVALID_THIS('StreamEvent');
    return this[kData].stream;
  }

  get respondWith() {
    if (!isStreamEvent(this))
      throw new ERR_INVALID_THIS('StreamEvent');
    return this[kData].stream.unidirectional ?
      undefined :
      (response) => this[kData].stream[kRespondWith](response);
  }

  [kInspect](depth, options) {
    if (!isStreamEvent(this))
      throw new ERR_INVALID_THIS('StreamEvent');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `${this[kType]} ${inspect({
      stream: this[kData].stream,
    }, opts)}`;
  }
}

ObjectDefineProperties(DatagramEvent.prototype, {
  stream: kEnumerableProperty,
  respondWith: kEnumerableProperty,
});

function isStreamEvent(value) {
  return typeof value?.[kData] === 'object' &&
         value?.[kType] === 'StreamEvent';
}

function createStreamEvent(stream) {
  return ReflectConstruct(
    class extends Event {
      constructor() {
        super('stream');
        this[kType] = 'StreamEvent';
        this[kData] = {
          stream,
        };
      }
    },
    [],
    StreamEvent);
}

function createLogStream(handle) {
  const readable = new Readable({
    read() {
      if (handle !== undefined)
        handle.readStart();
    },

    destroy() {
      handle[owner_symbol] = undefined;
      handle = undefined;
    },
  });
  readable[kUpdateTimer] = () => {};
  handle[owner_symbol] = readable;
  handle.onread = onStreamRead;
  return readable;
}

class StreamWritableSource extends Writable {
  constructor() {
    super();
    this[kHandle] = new StreamSource();
    this[kHandle][owner_symbol] = this;
  }

  _write(chunk, encoding, callback) {
    if (typeof chunk === 'string')
      chunk = Buffer.from(chunk, encoding);
    this[kHandle].write(chunk);
    callback();
  }

  _writev(data, callback) {
    this[kHandle].write(data);
    callback();
  }

  _final(callback) {
    this[kHandle].end();
    callback();
  }
}

// Used as the underlying source for a WritableStream
// TODO(@jasnell): Use the WritableStream StreamBase
// adapter here instead?
class WritableStreamSource {
  constructor() {
    this[kHandle] = new StreamSource();
    this[kHandle][owner_symbol] = this;
  }

  write(chunk) {
    if (!isAnyArrayBuffer(chunk))
      throw new ERR_INVALID_ARG_TYPE('chunk', 'ArrayBuffer', chunk);
    this[kHandle].write(chunk);
  }

  close() { this[kHandle].end(); }

  abort() { this[kHandle].end(); }
}

function getGeneratorSource(generator, stream) {
  const source = new StreamSource();
  source[owner_symbol] = generator;

  async function consume() {
    // Seems a bit odd here but this is the equivalent to a process.nextTick,
    // deferring the consumption of the generator until after the stream is
    // fully set up.
    await undefined;
    const enc = new TextEncoder();
    for await (let chunk of generator) {
      if (typeof chunk === 'string')
        chunk = enc.encode(chunk);
      if (!isAnyArrayBuffer(chunk) && !isArrayBufferView(chunk)) {
        throw new ERR_INVALID_ARG_TYPE(
          'chunk', [
            'string',
            'ArrayBuffer',
            'TypedArray',
            'Buffer',
            'DataView',
          ], chunk);
      }
      source.write(chunk);
    }
    source.end();
  }

  PromisePrototypeThen(
    consume(),
    undefined,
    (error) => stream[kDestroy](error));

  return source;
}

function acquireTrailers(trailers) {
  if (typeof trailers === 'function') {
    try {
      trailers = FunctionPrototypeCall(trailers);
    } catch (error) {
      return PromiseReject(error);
    }
  }

  if (trailers == null)
    return PromiseResolve(undefined);

  if (typeof trailers.then === 'function') {
    return PromisePrototypeThen(
      isPromise(trailers) ? trailers : PromiseResolve(trailers),
      acquireTrailers);
  }

  if (typeof trailers !== 'object') {
    return PromiseReject(
      new ERR_INVALID_ARG_TYPE('trailers', ['Object'], trailers));
  }

  return PromiseResolve(trailers);
}

// Body here can be one of:
// 1. Undefined/null
// 2. String
// 3. ArrayBuffer
// 4. ArrayBufferView (TypedArray, Buffer, DataView)
// 5. Blob
// 6. stream.Readable
// 7. ReadableStream
// 8. FileHandle
// 9. Generator Object (async or sync) yielding string, ArrayBuffer,
//    or ArrayBufferView
// 10. Async or Sync Iterable yielding string, ArrayBuffer, or
//     ArrayBufferView
// 11. A synchronous function returning 1-10
// 12. An asynchronous function resolving 1-10
//
// Regardless of what kind of thing body is, acquireBody
// always returns a promise that fulfills with the acceptable
// body value or rejects with an ERR_INVALID_ARG_TYPE.
function acquireBody(body, stream) {
  if (typeof body === 'function') {
    try {
      body = FunctionPrototypeCall(body);
    } catch (error) {
      return PromiseReject(error);
    }
  }

  if (body == null)
    return PromiseResolve(undefined);

  // If body is a thenable, we're going to let it
  // fulfill then try acquireBody again on the result.
  // If the thenable rejects, go ahead and let that
  // bubble up to the caller for handling.
  if (typeof body.then === 'function') {
    return PromisePrototypeThen(
      isPromise(body) ? body : PromiseResolve(body),
      acquireBody);
  }

  if (typeof body === 'string') {
    const enc = new TextEncoder();
    const source = new ArrayBufferViewSource(enc.encode(body));
    return PromiseResolve(source);
  }

  if (isGeneratorObject(body))
    return PromiseResolve(getGeneratorSource(body, stream));

  if (isAnyArrayBuffer(body)) {
    const source = new ArrayBufferViewSource(new Uint8Array(body));
    return PromiseResolve(source);
  }

  if (isArrayBufferView(body)) {
    const source = new ArrayBufferViewSource(body);
    return PromiseResolve(source);
  }

  if (isBlob(body)) {
    const source = new BlobSource(body[kBlobHandle]);
    return PromiseResolve(source);
  }

  if (isFileHandle(body)) {
    const source = new StreamBaseSource(body[kFileHandle]);
    PromisePrototypeFinally(
      PromisePrototypeThen(
        source.done,
        undefined,
        () => {
          // TODO(@jasnell): What to do with this error?
        }),
      () => body.close());
    return PromiseResolve(source);
  }

  if (isReadableStream(body)) {
    const source = new WritableStreamSource();
    const writable = new WritableStream(source);
    source.writable = writable;
    PromisePrototypeThen(
      body.pipeTo(writable),
      undefined,  // Do nothing on success
      (error) => stream[kDestroy](error));
    return PromiseResolve(source[kHandle]);
  }

  if (typeof body._readableState === 'object') {
    const promise = createDeferredPromise();
    const writable = new StreamWritableSource();
    pipeline(body, writable, (error) => {
      if (error) return promise.reject(error);
      promise.resolve();
    });
    // TODO(@jasnell): How to best surface this error?
    PromisePrototypeThen(
      promise.promise,
      undefined,  // Do nothing on success
      (error) => stream[kDestroy](error));
    stream[kHandle].attachSource(writable[kHandle]);
    return PromiseResolve(writable[kHandle]);
  }

  if (typeof body[SymbolIterator] === 'function') {
    try {
      return PromiseResolve(
        getGeneratorSource(body[SymbolIterator](), stream));
    } catch (error) {
      return PromiseReject(error);
    }
  }

  if (typeof body[SymbolAsyncIterator] === 'function') {
    try {
      return PromiseResolve(
        getGeneratorSource(body[SymbolAsyncIterator](), stream));
    } catch (error) {
      return PromiseReject(error);
    }
  }

  return PromiseReject(
    new ERR_INVALID_ARG_TYPE(
      'options.body',
      [
        'string',
        'ArrayBuffer',
        'TypedArray',
        'Buffer',
        'DataView',
        'Blob',
        'FileHandle',
        'ReadableStream',
        'stream.Readable',
        'Promise',
        'Function',
        'AsyncFunction',
      ],
      body));
}

function isPromisePending(promise) {
  if (promise === undefined) return false;
  const details = getPromiseDetails(promise);
  return details?.[0] === kPending;
}

function setPromiseHandled(promise) {
  PromisePrototypeThen(promise, undefined, () => {});
}

module.exports = {
  DatagramEvent,
  SessionEvent,
  StreamEvent,
  acquireBody,
  acquireTrailers,
  createDatagramEvent,
  createSessionEvent,
  createStreamEvent,
  createLogStream,
  isPromisePending,
  setPromiseHandled,
  kAddSession,
  kRemoveSession,
  kAddStream,
  kBlocked,
  kClientHello,
  kClose,
  kCreatedStream,
  kDestroy,
  kHandshakeComplete,
  kHeaders,
  kMaybeStreamEvent,
  kOCSP,
  kRemoveStream,
  kResetStream,
  kRespondWith,
  kSessionTicket,
  kSetSource,
  kState,
  kTrailers,
  kType,
};
