'use strict';

// Internal, experimental HTTP/3 consumer layer over node:quic.
//
// Http3Session wraps a QuicSession and surfaces incoming/outgoing request
// streams as Http3Stream objects.

const {
  SymbolAsyncIterator,
} = primordials;

const {
  connect: quicConnect,
  listen: quicListen,
  QuicSession,
  QuicStream,
} = require('internal/quic/quic');

const {
  validateFunction,
  validateObject,
} = require('internal/validators');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const kEmptyObject = { __proto__: null };
const kHttp3Alpn = 'h3';

function isQuicSession(value) {
  return value instanceof QuicSession;
}

function isQuicStream(value) {
  return value instanceof QuicStream;
}

class Http3Stream {
  #stream;
  #session;

  /**
   * @param {QuicStream} stream the underlying QUIC stream
   * @param {Http3Session} session the owning session wrapper
   */
  constructor(stream, session) {
    if (!isQuicStream(stream)) {
      throw new ERR_INVALID_ARG_TYPE('stream', 'QuicStream', stream);
    }
    this.#stream = stream;
    this.#session = session;
  }

  /** @type {Http3Session} */
  get session() { return this.#session; }

  /** @type {bigint} */
  get id() { return this.#stream.id; }

  /** @type {'bidi'|'uni'} */
  get direction() { return this.#stream.direction; }

  // Received header block (after onheaders fires).
  get headers() { return this.#stream.headers; }

  get onheaders() { return this.#stream.onheaders; }
  set onheaders(fn) { this.#stream.onheaders = fn; }

  get oninfo() { return this.#stream.oninfo; }
  set oninfo(fn) { this.#stream.oninfo = fn; }

  get ontrailers() { return this.#stream.ontrailers; }
  set ontrailers(fn) { this.#stream.ontrailers = fn; }

  get onwanttrailers() { return this.#stream.onwanttrailers; }
  set onwanttrailers(fn) { this.#stream.onwanttrailers = fn; }

  get onreset() { return this.#stream.onreset; }
  set onreset(fn) { this.#stream.onreset = fn; }

  get onerror() { return this.#stream.onerror; }
  set onerror(fn) { this.#stream.onerror = fn; }

  sendHeaders(headers, options = kEmptyObject) {
    return this.#stream.sendHeaders(headers, options);
  }

  sendInformationalHeaders(headers) {
    return this.#stream.sendInformationalHeaders(headers);
  }

  sendTrailers(headers) {
    return this.#stream.sendTrailers(headers);
  }

  // Outbound body writer (stream/iter push writer).
  get writer() { return this.#stream.writer; }

  // Inbound body bytes; ends at FIN.
  [SymbolAsyncIterator]() {
    return this.#stream[SymbolAsyncIterator]();
  }

  /** @type {Promise<void>} */
  get closed() { return this.#stream.closed; }

  get destroyed() { return this.#stream.destroyed; }

  destroy(error, options) { return this.#stream.destroy(error, options); }

  stopSending(code) { return this.#stream.stopSending(code); }

  resetStream(code) { return this.#stream.resetStream(code); }
}

class Http3Session {
  #session;
  #onstream = undefined;

  /**
   * Wraps an existing (built-in) QuicSession. Must be constructed
   * synchronously in the frame the session is delivered, before any I/O
   * tick, so that no incoming stream is missed.
   * @param {QuicSession} session the QUIC session to wrap
   */
  constructor(session) {
    if (!isQuicSession(session)) {
      // Foreign QuicLike transports take the slow path; that adapter is not
      // wired up yet, so only built-in sessions are accepted for now.
      throw new ERR_INVALID_ARG_TYPE('session', 'QuicSession', session);
    }
    this.#session = session;
    session.onstream = (stream) => {
      const wrapped = new Http3Stream(stream, this);
      this.#onstream?.(wrapped);
    };
  }

  // The underlying QUIC session (transitional escape hatch).
  get session() { return this.#session; }

  get onstream() { return this.#onstream; }
  set onstream(fn) {
    if (fn !== undefined) validateFunction(fn, 'onstream');
    this.#onstream = fn;
  }

  get ongoaway() { return this.#session.ongoaway; }
  set ongoaway(fn) { this.#session.ongoaway = fn; }

  get onorigin() { return this.#session.onorigin; }
  set onorigin(fn) { this.#session.onorigin = fn; }

  get onerror() { return this.#session.onerror; }
  set onerror(fn) { this.#session.onerror = fn; }

  /** @type {Promise<object>} */
  get opened() { return this.#session.opened; }

  /** @type {Promise<void>} */
  get closed() { return this.#session.closed; }

  /**
   * Opens a request stream: a bidirectional stream carrying the given
   * request headers (and optional body via options.body).
   * @param {object} headers the request header block
   * @param {object} [options] stream options (body, callbacks, ...)
   * @returns {Promise<Http3Stream>}
   */
  async createRequestStream(headers, options = kEmptyObject) {
    validateObject(headers, 'headers');
    validateObject(options, 'options');
    const stream = await this.#session.createBidirectionalStream({
      ...options,
      headers,
    });
    return new Http3Stream(stream, this);
  }

  close(options) { return this.#session.close(options); }

  destroy(error, options) { return this.#session.destroy(error, options); }
}

/**
 * Connects a built-in QUIC session with ALPN h3 and wraps it.
 * @returns {Promise<Http3Session>}
 */
async function connect(address, options = kEmptyObject) {
  validateObject(options, 'options');
  const session = await quicConnect(address, {
    ...options,
    alpn: kHttp3Alpn,
  });
  return new Http3Session(session);
}

/**
 * Listens with ALPN h3; onsession receives an Http3Session, wrapped
 * synchronously in the delivery frame per the attach contract.
 * @param {Function} onsession invoked with each new Http3Session
 * @param {object} [options] quic listen options
 * @returns {Promise<object>} the listening QuicEndpoint
 */
async function listen(onsession, options = kEmptyObject) {
  validateFunction(onsession, 'onsession');
  validateObject(options, 'options');
  return quicListen((session) => {
    onsession(new Http3Session(session));
  }, { ...options, alpn: kHttp3Alpn });
}

module.exports = {
  Http3Session,
  Http3Stream,
  connect,
  listen,
};
