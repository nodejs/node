'use strict';

const {
  Boolean,
  DataView,
  ObjectAssign,
  ObjectCreate,
  ObjectDefineProperties,
  PromisePrototypeThen,
  PromiseReject,
  ReflectConstruct,
} = primordials;

const {
  createEndpoint: _createEndpoint,
  JSQuicBufferConsumer,
  QUIC_STREAM_HEADERS_KIND_INFO,
  QUIC_STREAM_HEADERS_KIND_INITIAL,
  QUIC_STREAM_HEADERS_KIND_TRAILING,
  IDX_STATE_STREAM_FIN_SENT,
  IDX_STATE_STREAM_FIN_RECEIVED,
  IDX_STATE_STREAM_READ_ENDED,
  IDX_STATE_STREAM_TRAILERS,
} = internalBinding('quic');

// If the _createEndpoint is undefined, the Node.js binary
// was built without QUIC support, in which case we
// don't want to export anything here.
if (_createEndpoint === undefined)
  return;

const {
  acquireBody,
  acquireTrailers,
  isPromisePending,
  setPromiseHandled,
  kBlocked,
  kDestroy,
  kHeaders,
  kMaybeStreamEvent,
  kResetStream,
  kRespondWith,
  kState,
  kTrailers,
  kType,
} = require('internal/quic/common');

const {
  kHandle,
  ResponseOptions,
} = require('internal/quic/config');

const {
  symbols: {
    owner_symbol,
  },
} = require('internal/async_hooks');

const {
  kEnumerableProperty,
} = require('internal/webstreams/util');

const {
  createStreamStats,
  kDetach: kDetachStats,
  kStats,
} = require('internal/quic/stats');

const {
  createDeferredPromise,
  customInspectSymbol: kInspect,
} = require('internal/util');

const {
  Readable,
} = require('stream');

const {
  inspect,
} = require('util');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
    ERR_INVALID_THIS,
    ERR_QUIC_STREAM_RESET,
  },
} = require('internal/errors');

const {
  ReadableStream,
  readableStreamCancel,
  isReadableStream,
} = require('internal/webstreams/readablestream');

const {
  toHeaderObject,
} = require('internal/http2/util');

class StreamState {
  constructor(state) {
    this[kHandle] = new DataView(state);
    this.wrapped = true;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get finSent() {
    return this[kHandle].getUint8(IDX_STATE_STREAM_FIN_SENT) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get finReceived() {
    return this[kHandle].getUint8(IDX_STATE_STREAM_FIN_RECEIVED) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get readEnded() {
    return this[kHandle].getUint8(IDX_STATE_STREAM_READ_ENDED) === 1;
  }


  /**
   * @type {boolean}
   */
  get trailers() {
    return this[kHandle].getUint8(IDX_STATE_STREAM_TRAILERS) === 1;
  }

  /**
   * @type {boolean}
   */
  set trailers(val) {
    this[kHandle].setUint8(IDX_STATE_STREAM_TRAILERS, val ? 1 : 0);
  }
}

class Stream {
  constructor() { throw new ERR_ILLEGAL_CONSTRUCTOR(); }

  [kBlocked]() {
    blockedStream(this);
  }

  [kDestroy](error) {
    destroyStream(this, error);
  }

  [kHeaders](headers, kind) {
    streamHeaders(this, headers, kind);
  }

  [kResetStream](appErrorCode) {
    // A stream reset is a terminal state. If appErrorCode
    // is equal to NO_ERROR (0), then we simply destroy
    // the stream with no error and treat it as a normal
    // cancel. Any other error code triggers an error flow.
    let error;
    if (appErrorCode !== 0)
      error = new ERR_QUIC_STREAM_RESET(appErrorCode);
    destroyStream(this, error);
  }

  [kRespondWith](response = new ResponseOptions()) {
    if (!isStream(this))
      PromiseReject(new ERR_INVALID_THIS('Stream'));
    respondWith(this, response);
  }

  // Called only when the system is ready to send trailers
  [kTrailers]() {
    const application = this[kState].session[kState].application;
    const handle = this[kHandle];
    if (this[kState].trailerSource !== undefined) {
      PromisePrototypeThen(
        acquireTrailers(this[kState].trailerSource),
        (trailers) => application.handleTrailingHeaders(handle, trailers),
        (error) => destroyStream(this, error));
    } else {
      application.handleTrailingHeaders(handle, {});
    }
  }

  /**
   * A promise that is fulfilled when the Stream is
   * reported as being blocked. Whenever blocked is
   * fulfilled, a new promise is created.
   * @type {Promise<any>}
   */
  get blocked() {
    if (!isStream(this))
      return PromiseReject(new ERR_INVALID_THIS('Stream'));
    return this[kState].blocked.promise;
  }

  /**
   * A promise that is fulfilled when the Stream has
   * been closed. If the Stream closed normally, the
   * promise will be fulfilled with undefined. If the
   * Stream closed abnormally, the promise will be
   * rejected with a reason indicating why.
   * @readonly
   * @type {Promise<any>}
   */
  get closed() {
    if (!isStream(this))
      return PromiseReject(new ERR_INVALID_THIS('Stream'));
    return this[kState].closed.promise;
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get id() {
    if (!isStream(this))
      throw new ERR_INVALID_THIS('Stream');
    return this[kState].id;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get unidirectional() {
    if (!isStream(this))
      throw new ERR_INVALID_THIS('Stream');
    return isStreamUnidirectional(this);
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get serverInitiated() {
    if (!isStream(this))
      throw new ERR_INVALID_THIS('Stream');
    return isStreamServerInitiated(this);
  }

  /**
   * Called by user-code to signal no further interest
   * in the Stream. It will be immediately destroyed.
   * Any data pending in the outbound and inbound queues
   * will be abandoned.
   * @param {any} [reason]
   */
  cancel(reason) {
    if (!isStream(this))
      throw new ERR_INVALID_THIS('Stream');
    destroyStream(this, reason);
  }

  /**
   * @readonly
   * @type {import('./stats').StreamStats}
   */
  get stats() {
    if (!isStream(this))
      throw new ERR_INVALID_THIS('Stream');
    return this[kStats];
  }

  /**
   * @readonly
   * @type {boolean} Set to `true` if there is an active consumer.
   */
  get locked() {
    if (!isStream(this))
      throw new ERR_INVALID_THIS('Stream');
    return isStreamLocked(this);
  }

  /**
   * @typedef {import('stream/web').ReadableStream} ReadableStream
   * @returns {ReadableStream}
   */
  readableWebStream() {
    if (!isStream(this))
      throw new ERR_INVALID_THIS('Stream');
    if (isStreamDestroyed(this))
      throw new ERR_INVALID_STATE('Stream is already destroyed');
    if (isStreamLocked(this))
      throw new ERR_INVALID_STATE('Stream is already being consumed');

    return acquireReadableStream(this);
  }

  /**
   * @returns {Readable}
   */
  readableNodeStream() {
    if (!isStream(this))
      throw new ERR_INVALID_THIS('Stream');
    if (isStreamDestroyed(this))
      throw new ERR_INVALID_STATE('Stream is already destroyed');
    if (isStreamLocked(this))
      throw new ERR_INVALID_STATE('Stream is already being consumed');

    return acquireStreamReadable(this);
  }

  get session() {
    if (!isStream(this))
      throw new ERR_INVALID_THIS('Stream');
    return this[kState].session;
  }

  /**
   * @type {Promise<{}>} The headers associated with this Stream, if any
   */
  get headers() {
    if (!isStream(this))
      throw new ERR_INVALID_THIS('Stream');
    return this[kState].headers.promise;
  }

  /**
   * @type {Promise<{}>} The trailers associated with this Stream, if any
   */
  get trailers() {
    if (!isStream(this))
      return PromiseReject(new ERR_INVALID_THIS('Stream'));
    return this[kState].trailers.promise;
  }

  [kInspect](depth, options) {
    if (!isStream(this))
      throw new ERR_INVALID_THIS('Stream');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `${this[kType]} ${inspect({
      closed: this[kState].closed.promise,
      destroyed: isStreamDestroyed(this),
      id: this[kState].id,
      headers: this[kState].headers,
      locked: isStreamLocked(this),
      serverInitiated: isStreamServerInitiated(this),
      session: this[kState].session,
      stats: this[kStats],
      trailers: this[kState].trailers.promise,
      unidirectional: isStreamUnidirectional(this),
    }, opts)}`;
  }
}

ObjectDefineProperties(Stream.prototype, {
  blocked: kEnumerableProperty,
  closed: kEnumerableProperty,
  cancel: kEnumerableProperty,
  headers: kEnumerableProperty,
  id: kEnumerableProperty,
  locked: kEnumerableProperty,
  readableStream: kEnumerableProperty,
  respondWith: kEnumerableProperty,
  serverInitiated: kEnumerableProperty,
  session: kEnumerableProperty,
  stats: kEnumerableProperty,
  streamReadable: kEnumerableProperty,
  trailers: kEnumerableProperty,
  unidirectional: kEnumerableProperty,
});

function createStream(handle, session, trailerSource) {
  const ret = ReflectConstruct(
    function() {
      this[kType] = 'Stream';
      const inner = new StreamState(handle.state);
      this[kState] = {
        id: handle.id,
        inner,
        blocked: undefined,
        hints: ObjectCreate(null),
        closed: createDeferredPromise(),
        headers: createDeferredPromise(),
        trailers: createDeferredPromise(),
        consumer: undefined,
        destroyed: false,
        responded: false,
        session,
        trailerSource,
      };
      this[kHandle] = handle;
      this[kStats] = createStreamStats(handle.stats);
      inner.trailers = trailerSource !== undefined;
      setNewBlockedPromise(this);
      setPromiseHandled(this[kState].closed.promise);
      setPromiseHandled(this[kState].trailers.promise);
      setPromiseHandled(this[kState].headers.promise);
    },
    [],
    Stream);
  ret[kHandle][owner_symbol] = ret;
  return ret;
}

function isStream(value) {
  return typeof value?.[kState] === 'object' && value?.[kType] === 'Stream';
}

function isStreamLocked(stream) {
  return stream[kState].consumer !== undefined;
}

function isStreamDestroyed(stream) {
  return stream[kHandle] === undefined;
}

function isStreamUnidirectional(stream) {
  return Boolean(stream[kState].id & 0b10n);
}

function isStreamServerInitiated(stream) {
  return Boolean(stream[kState].id & 0b01n);
}

function setStreamSource(stream, source) {
  if (isStreamDestroyed(stream))
    throw new ERR_INVALID_STATE('Stream is already destroyed');
  stream[kHandle].attachSource(source);
}

function setNewBlockedPromise(stream) {
  stream[kState].blocked = createDeferredPromise();
  setPromiseHandled(stream[kState].blocked.promise);
}

function blockedStream(stream) {
  stream[kState].blocked.resolve();
  setNewBlockedPromise(stream);
}

function destroyStream(stream, error) {
  if (isStreamDestroyed(stream))
    return;

  const handle = stream[kHandle];
  stream[kHandle][owner_symbol] = undefined;
  stream[kHandle] = undefined;

  stream[kState].inner = undefined;
  stream[kStats][kDetachStats]();

  handle.destroy();

  const {
    blocked,
    consumer,
    closed,
  } = stream[kState];

  blocked.reject(new ERR_INVALID_STATE('Stream has been canceled'));

  if (typeof consumer?.destroy === 'function')
    consumer.destroy(error);
  else if (isReadableStream(consumer))
    readableStreamCancel(consumer, error);

  if (error)
    closed.reject(error);
  else
    closed.resolve();
}

async function respondWith(stream, response) {
  // Using a thenable here instead of a real Promise will have
  // a negative performance impact. We support both but a Promise
  // is better.
  if (typeof response?.then === 'function')
    response = await response;

  if (stream[kState].responded)
    throw new ERR_INVALID_STATE('A response has already been sent');

  if (!ResponseOptions.isResponseOptions(response)) {
    if (response === null || typeof response !== 'object') {
      throw new ERR_INVALID_ARG_TYPE('response', [
        'ResponseOptions',
        'Object',
      ], response);
    }
    response = new ResponseOptions(response);
  }

  const {
    hints,
    headers,
    trailers,
    body,
  } = response;

  const application = stream[kState].session[kState].application;

  stream[kState].trailerSource = trailers;
  stream[kState].inner.trailers = trailers !== undefined;

  try {
    application.handleHints(stream[kHandle], hints);
    const actualBody = await acquireBody(body);
    await application.handleResponseHeaders(
      stream[kHandle],
      headers,
      actualBody === undefined);
    setStreamSource(stream, actualBody);
    stream[kState].responded = true;
  } catch (error) {
    destroyStream(stream, error);
  }
}

function acquireStreamReadable(stream) {
  const handle = new JSQuicBufferConsumer();
  handle.emit = (chunks, done) => {
    for (let n = 0; n < chunks.length; n++)
      stream[kState].consumer.push(chunks[n]);
    if (done)
      stream[kState].consumer.push(null);
  };
  stream[kState].consumer = new Readable({
    [kHandle]: handle,
    read(size) {
      // Nothing to do here, the data should already
      // be flowing if it's available.
    }
  });
  stream[kHandle].attachConsumer(handle);
  return stream[kState].consumer;
}

function acquireReadableStream(stream) {
  let controller;
  const handle = new JSQuicBufferConsumer();
  handle.emit = (chunks, done) => {
    for (let n = 0; n < chunks.length; n++)
      controller.enqueue(chunks[n]);
    if (done)
      controller.close();
  };
  stream[kState].consumer = new ReadableStream({
    start(c) { controller = c; }
  });
  stream[kHandle].attachConsumer(handle);
  return stream[kState].consumer;
}

function streamHeaders(stream, headers, kind) {
  switch (kind) {
    case QUIC_STREAM_HEADERS_KIND_INFO:
      if (isPromisePending(stream[kState].headers.promise))
        stream[kState].hints =
          ObjectAssign(stream[kState].hints, toHeaderObject(headers));
      break;
    case QUIC_STREAM_HEADERS_KIND_INITIAL:
      if (isPromisePending(stream[kState].headers.promise)) {
        stream[kState].headers.resolve(
          ObjectAssign(stream[kState].hints, toHeaderObject(headers)));
      }
      stream[kState].session[kMaybeStreamEvent](stream);
      break;
    case QUIC_STREAM_HEADERS_KIND_TRAILING:
      if (isPromisePending(stream[kState].trailers.promise))
        stream[kState].trailers.resolve(toHeaderObject(headers));
      break;
  }
}

module.exports = {
  Stream,
  createStream,
  destroyStream,
  isStream,
  setStreamSource,
};
