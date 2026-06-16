'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  ObjectHasOwn,
  Symbol,
  SymbolAsyncIterator,
} = primordials;

const {
  getOptionValue,
} = require('internal/options');

// HTTP/3 requires QUIC, which requires that Node.js be compiled with
// crypto support and that the --experimental-quic flag be enabled.
if (!process.features.quic || !getOptionValue('--experimental-quic')) {
  return;
}

// Internal, experimental HTTP/3 consumer layer over node:quic.
//
// Http3Session wraps a QuicSession and surfaces incoming/outgoing request
// streams as Http3Stream objects.

const {
  connect: quicConnect,
  listen: quicListen,
  QuicSession,
  QuicStream,
  kApplication,
  kApplicationSettings,
  kStreamHandle,
  kSessionHandle,
  markSessionClosing,
  getQuicStreamState,
  getQuicSessionState,
  safeCallbackInvoke: quicSafeCallbackInvoke,
  kApplicationOwner,
} = require('internal/quic/quic');

const {
  setHttp3Callbacks,
  createHttp3Handle,

  QUIC_STREAM_HEADERS_KIND_INITIAL: kHeadersKindInitial,
  QUIC_STREAM_HEADERS_KIND_HINTS: kHeadersKindHints,
  QUIC_STREAM_HEADERS_KIND_TRAILING: kHeadersKindTrailing,
  QUIC_STREAM_HEADERS_FLAGS_NONE: kHeadersFlagsNone,
  QUIC_STREAM_HEADERS_FLAGS_TERMINAL: kHeadersFlagsTerminal,
} = internalBinding('quic');

const {
  buildNgHeaderString,
  assertValidPseudoHeader,
  assertValidPseudoHeaderTrailer,
} = require('internal/http2/util');

const {
  onSessionApplicationChannel,
  onSessionClosingChannel,
  onSessionGoawayChannel,
  onSessionOriginChannel,
  onStreamHeadersChannel,
  onStreamTrailersChannel,
  onStreamInfoChannel,
} = require('internal/quic/diagnostics');

const {
  validateBoolean,
  validateFunction,
  validateObject,
  validateOneOf,
} = require('internal/validators');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
    ERR_QUIC_OPEN_STREAM_FAILED,
  },
} = require('internal/errors');

const assert = require('internal/assert');

const kEmptyObject = { __proto__: null };
const kHttp3Alpn = 'h3';

// Module-private: routing of the HTTP/3 application events (registered
// via setHttp3Callbacks below) into the wrappers.
const kOnHeaders = Symbol('kOnHeaders');
const kOnWantTrailers = Symbol('kOnWantTrailers');
const kOnGoaway = Symbol('kOnGoaway');
const kOnOrigin = Symbol('kOnOrigin');
const kOnSettings = Symbol('kOnSettings');

function isQuicSession(value) {
  return value instanceof QuicSession;
}

function isQuicStream(value) {
  return value instanceof QuicStream;
}

/**
 * Parses an alternating [name, value, name, value, ...] array from the
 * application into a plain header object. Multi-value headers become arrays.
 * @param {string[]} pairs
 * @returns {object}
 */
function parseHeaderPairs(pairs) {
  assert(ArrayIsArray(pairs));
  assert(pairs.length % 2 === 0);
  const block = { __proto__: null };
  for (let n = 0; n + 1 < pairs.length; n += 2) {
    if (block[pairs[n]] !== undefined) {
      if (ArrayIsArray(block[pairs[n]])) {
        ArrayPrototypePush(block[pairs[n]], pairs[n + 1]);
      } else {
        block[pairs[n]] = [block[pairs[n]], pairs[n + 1]];
      }
    } else {
      block[pairs[n]] = pairs[n + 1];
    }
  }
  return block;
}

/**
 * @typedef {object} SendHeadersOptions
 * @property {boolean} [terminal] When true, indicates that no body data
 *   will be sent after these headers.
 */

const kGetHttp3Handle = Symbol('kGetHttp3Handle');
const kSubmitInitialHeaders = Symbol('kSubmitInitialHeaders');

function priorityFieldValue(level, incremental) {
  const urgency = level === 'high' ? 0 : level === 'low' ? 7 : 3;
  if (urgency === 3 && !incremental) return undefined;
  return incremental ? `u=${urgency}, i` : `u=${urgency}`;
}

class Http3Stream {
  #stream;
  #session;
  #h3handle;
  #headers = undefined;
  #onheaders = undefined;
  #oninfo = undefined;
  #ontrailers = undefined;
  #onwanttrailers = undefined;
  // Client-side stored priority (the value this side requested). The server
  // side reads the peer's requested priority from nghttp3 instead.
  #priority = { __proto__: null, level: 'default', incremental: false };
  // Have we submitted an initial header block (request/response) to nghttp3?
  #headersSubmitted = false;

  /**
   * @param {QuicStream} stream the underlying QUIC stream
   * @param {Http3Session} session the owning session wrapper
   * @param {object} [callbacks] creation-time stream callbacks
   */
  constructor(stream, session, callbacks = kEmptyObject) {
    if (!isQuicStream(stream)) {
      throw new ERR_INVALID_ARG_TYPE('stream', 'QuicStream', stream);
    }
    this.#stream = stream;
    this.#session = session;
    this.#h3handle = session[kGetHttp3Handle]();
    const handle = stream[kStreamHandle];
    if (handle !== undefined) {
      handle[kApplicationOwner] = this;
    }
    const {
      onheaders,
      oninfo,
      ontrailers,
      onwanttrailers,
      onreset,
      onerror,
    } = callbacks;
    if (onheaders !== undefined) this.onheaders = onheaders;
    if (oninfo !== undefined) this.oninfo = oninfo;
    if (ontrailers !== undefined) this.ontrailers = ontrailers;
    if (onwanttrailers !== undefined) this.onwanttrailers = onwanttrailers;
    if (onreset !== undefined) this.onreset = onreset;
    if (onerror !== undefined) this.onerror = onerror;
  }

  [kOnHeaders](pairs, kind) {
    if (this.#stream.destroyed) return;
    quicSafeCallbackInvoke(() => this.#onHeaderBlock(pairs, kind), this.#stream);
  }

  [kOnWantTrailers]() {
    if (this.#stream.destroyed) return;
    if (typeof this.#onwanttrailers !== 'function') return;
    quicSafeCallbackInvoke(() => this.#onwanttrailers(), this.#stream);
  }

  #onHeaderBlock(pairs, kind) {
    const block = parseHeaderPairs(pairs);
    const stream = this.#stream;
    switch (kind) {
      case kHeadersKindInitial:
        this.#headers ??= block;
        if (onStreamHeadersChannel.hasSubscribers) {
          onStreamHeadersChannel.publish({
            __proto__: null,
            stream,
            session: stream.session,
            headers: block,
          });
        }
        if (typeof this.#onheaders === 'function') {
          return this.#onheaders(block);
        }
        return undefined;
      case kHeadersKindTrailing:
        if (onStreamTrailersChannel.hasSubscribers) {
          onStreamTrailersChannel.publish({
            __proto__: null,
            stream,
            session: stream.session,
            trailers: block,
          });
        }
        if (typeof this.#ontrailers === 'function') {
          return this.#ontrailers(block);
        }
        return undefined;
      case kHeadersKindHints:
        if (onStreamInfoChannel.hasSubscribers) {
          onStreamInfoChannel.publish({
            __proto__: null,
            stream,
            session: stream.session,
            headers: block,
          });
        }
        if (typeof this.#oninfo === 'function') {
          return this.#oninfo(block);
        }
        return undefined;
    }
  }

  /** @type {Http3Session} */
  get session() { return this.#session; }

  /** @type {bigint} */
  get id() { return this.#stream.id; }

  /** @type {'bidi'|'uni'} */
  get direction() { return this.#stream.direction; }

  // Received header block (after onheaders fires).
  get headers() { return this.#headers; }

  // True when the stream's data was received as 0-RTT early data.
  get early() { return this.#stream.early; }

  get #isServer() {
    return getQuicSessionState(this.#session.session).isServer;
  }

  get #knownToNgHttp3() {
    return this.#isServer || this.#headersSubmitted;
  }

  // The stream's priority. On a client this is the value we requested; on a
  // server it is the peer's requested priority read from nghttp3.
  get priority() {
    const stream = this.#stream;
    if (stream.destroyed || this.#h3handle === undefined) return null;
    if (!this.#isServer) {
      return { level: this.#priority.level,
               incremental: this.#priority.incremental };
    }
    const packed = this.#h3handle.getPriority(stream[kStreamHandle]);
    if (packed === undefined) return null;
    const urgency = packed >> 1;
    const incremental = !!(packed & 1);
    const level = urgency < 3 ? 'high' : urgency > 3 ? 'low' : 'default';
    return { level, incremental };
  }

  setPriority(options = kEmptyObject) {
    const stream = this.#stream;
    if (stream.destroyed) return;
    validateObject(options, 'options');
    const { level = 'default', incremental = false } = options;
    validateOneOf(level, 'options.level', ['default', 'low', 'high']);
    validateBoolean(incremental, 'options.incremental');
    this.#priority = { __proto__: null, level, incremental };

    // Before the stream is known to nghttp3 (client, pre-submit) the requested
    // priority is carried in the initial header block by sendHeaders;
    // afterwards a change is signaled with a PRIORITY_UPDATE frame.
    if (this.#knownToNgHttp3 && this.#h3handle !== undefined) {
      const urgency = level === 'high' ? 0 : level === 'low' ? 7 : 3;
      this.#h3handle.setPriority(
        stream[kStreamHandle], (urgency << 1) | (incremental ? 1 : 0));
    }
  }

  [kSubmitInitialHeaders](headerString, flags) {
    const stream = this.#stream;
    if (stream.destroyed || this.#h3handle === undefined) return false;
    this.#headersSubmitted = true;
    return this.#h3handle.sendHeaders(
      stream[kStreamHandle], headerString, flags);
  }

  #updateHeaderInterest() {
    getQuicStreamState(this.#stream).wantsHeaders =
      this.#onheaders !== undefined ||
      this.#oninfo !== undefined ||
      this.#ontrailers !== undefined;
  }

  get onheaders() { return this.#onheaders; }
  set onheaders(fn) {
    if (fn !== undefined) validateFunction(fn, 'onheaders');
    this.#onheaders = fn;
    this.#updateHeaderInterest();
  }

  get oninfo() { return this.#oninfo; }
  set oninfo(fn) {
    if (fn !== undefined) validateFunction(fn, 'oninfo');
    this.#oninfo = fn;
    this.#updateHeaderInterest();
  }

  get ontrailers() { return this.#ontrailers; }
  set ontrailers(fn) {
    if (fn !== undefined) validateFunction(fn, 'ontrailers');
    this.#ontrailers = fn;
    this.#updateHeaderInterest();
  }

  get onwanttrailers() { return this.#onwanttrailers; }
  set onwanttrailers(fn) {
    if (fn === undefined) {
      this.#onwanttrailers = undefined;
      // The application ends the stream after the body when no trailers
      // are expected.
      getQuicStreamState(this.#stream).wantsTrailers = false;
    } else {
      validateFunction(fn, 'onwanttrailers');
      this.#onwanttrailers = fn;
      // Tell the application to keep the stream open after the body so
      // trailers can be submitted.
      getQuicStreamState(this.#stream).wantsTrailers = true;
    }
  }

  get onreset() { return this.#stream.onreset; }
  set onreset(fn) { this.#stream.onreset = fn; }

  get onerror() { return this.#stream.onerror; }
  set onerror(fn) { this.#stream.onerror = fn; }

  /**
   * Sends the initial request or response header block.
   * @param {object} headers
   * @param {SendHeadersOptions} [options]
   * @returns {boolean} true if the headers were scheduled to be sent.
   */
  sendHeaders(headers, options = kEmptyObject) {
    const stream = this.#stream;
    if (stream.destroyed || this.#h3handle === undefined) return false;
    validateObject(headers, 'headers');
    const { terminal = false } = options;

    // A client request carries its requested priority as a priority header
    // when set - server responses signal via setPriority instead.
    let toSend = headers;
    if (!this.#isServer) {
      const pri = priorityFieldValue(
        this.#priority.level, this.#priority.incremental);
      if (pri !== undefined && !ObjectHasOwn(headers, 'priority')) {
        toSend = { __proto__: null, ...headers, priority: pri };
      }
    }

    const headerString = buildNgHeaderString(
      toSend, assertValidPseudoHeader, true /* strictSingleValueFields */);
    const flags = terminal ? kHeadersFlagsTerminal : kHeadersFlagsNone;
    // The stream now exists in nghttp3; later priority changes use
    // PRIORITY_UPDATE rather than the header block.
    this.#headersSubmitted = true;
    return this.#h3handle.sendHeaders(
      stream[kStreamHandle], headerString, flags);
  }

  /**
   * Sends informational (1xx) headers on this stream. Server only.
   * @param {object} headers
   * @returns {boolean} true if the headers were scheduled to be sent.
   */
  sendInformationalHeaders(headers) {
    const stream = this.#stream;
    if (stream.destroyed) return false;
    if (this.#h3handle === undefined) return false;
    validateObject(headers, 'headers');
    const headerString = buildNgHeaderString(
      headers, assertValidPseudoHeader, true);
    return this.#h3handle.sendInformationalHeaders(
      stream[kStreamHandle], headerString);
  }

  /**
   * Sends trailing headers on this stream. Must be called synchronously
   * during the onwanttrailers callback.
   * @param {object} headers
   * @returns {boolean} true if the trailers were scheduled to be sent.
   */
  sendTrailers(headers) {
    const stream = this.#stream;
    if (stream.destroyed) return false;
    if (this.#h3handle === undefined) return false;
    validateObject(headers, 'headers');
    const headerString =
      buildNgHeaderString(headers, assertValidPseudoHeaderTrailer);
    return this.#h3handle.sendTrailers(
      stream[kStreamHandle], headerString);
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
  #h3handle;
  #onstream = undefined;
  #ongoaway = undefined;
  #onorigin = undefined;
  #onsettings = undefined;

  /**
   * Wraps an existing (built-in) QuicSession. Must be constructed
   * synchronously in the frame the session is delivered, before any I/O
   * tick, so that no incoming stream or session event is missed.
   * @param {QuicSession} session the QUIC session to wrap
   * @param {object} [callbacks] creation-time session callbacks
   */
  constructor(session, callbacks = kEmptyObject) {
    if (!isQuicSession(session)) {
      // Foreign QuicLike transports take the slow path; that adapter is not
      // wired up yet, so only built-in sessions are accepted for now.
      throw new ERR_INVALID_ARG_TYPE('session', 'QuicSession', session);
    }
    this.#session = session;
    // Receive the raw HTTP/3 application events for this session (GOAWAY,
    // ORIGIN): they are emitted on the session's binding handle and
    // routed back here.
    const handle = session[kSessionHandle];
    if (handle !== undefined) {
      if (handle[kApplicationOwner] !== undefined) {
        throw new ERR_INVALID_STATE(
          'The QUIC session already has an application attached');
      }
      this.#h3handle = createHttp3Handle(handle);
      handle[kApplicationOwner] = this;
    }
    // Claim the session's incoming streams. This takes the single
    // public onstream slot; a tap chains it explicitly (grab the
    // previous handler and call both).
    session.onstream = (stream) => {
      if (typeof this.#onstream !== 'function') {
        process.emitWarning(
          'A new HTTP/3 stream was received but no onstream callback ' +
          'was provided');
        stream.destroy();
        return;
      }

      return this.#onstream(new Http3Stream(stream, this));
    };
    const { ongoaway, onorigin, onsettings } = callbacks;
    if (ongoaway !== undefined) this.ongoaway = ongoaway;
    if (onorigin !== undefined) this.onorigin = onorigin;
    if (onsettings !== undefined) this.onsettings = onsettings;
  }

  // Internal: hands the per-session HTTP/3 handle to this session's streams.
  [kGetHttp3Handle]() { return this.#h3handle; }

  /**
   * The peer initiated a graceful shutdown of the session (HTTP/3
   * GOAWAY). The session stops allowing new streams.
   * @param {bigint} lastStreamId The highest stream ID the peer may
   *   have processed - streams above it were not processed and may be
   *   retried.
   */
  [kOnGoaway](lastStreamId) {
    const session = this.#session;
    if (session.destroyed) return;
    markSessionClosing(session);
    if (onSessionClosingChannel.hasSubscribers) {
      onSessionClosingChannel.publish({ __proto__: null, session });
    }
    if (onSessionGoawayChannel.hasSubscribers) {
      onSessionGoawayChannel.publish({
        __proto__: null,
        session,
        lastStreamId,
      });
    }
    if (typeof this.#ongoaway === 'function') {
      // A sync throw or async rejection from the user callback
      // destroys the session with that error.
      quicSafeCallbackInvoke(() => this.#ongoaway(lastStreamId), session);
    }
  }

  /**
   * The peer announced the origins it claims authority for (HTTP/3
   * ORIGIN frame).
   * @param {string[]} origins
   */
  [kOnOrigin](origins) {
    const session = this.#session;
    if (session.destroyed) return;
    if (onSessionOriginChannel.hasSubscribers) {
      onSessionOriginChannel.publish({
        __proto__: null,
        origins,
        session,
      });
    }
    if (typeof this.#onorigin === 'function') {
      quicSafeCallbackInvoke(() => this.#onorigin(origins), session);
    }
  }

  /**
   * The peer's HTTP/3 SETTINGS were received (or updated). The negotiated
   * values are now readable via `session.settings` and are passed to the
   * callback for convenience.
   */
  [kOnSettings]() {
    const session = this.#session;
    if (session.destroyed) return;
    const settings = this.settings;
    if (onSessionApplicationChannel.hasSubscribers) {
      onSessionApplicationChannel.publish({
        __proto__: null,
        settings,
        session,
      });
    }
    if (typeof this.#onsettings === 'function') {
      quicSafeCallbackInvoke(() => this.#onsettings(settings), session);
    }
  }

  // The underlying QUIC session (transitional escape hatch).
  get session() { return this.#session; }

  /**
   * The effective HTTP/3 settings for this session. Reflects the
   * configured values plus any updates received from the peer's
   * SETTINGS frame (e.g. enableConnectProtocol). Returns null once the
   * session is destroyed.
   * @type {object|null}
   */
  get settings() {
    if (this.#session.destroyed) return null;
    // Not cached: the values can be updated by the peer's SETTINGS.
    return this.#session[kSessionHandle].applicationSettings();
  }

  get servername() { return this.#session.servername; }

  get alpnProtocol() { return this.#session.alpnProtocol; }

  get onstream() { return this.#onstream; }
  set onstream(fn) {
    if (fn !== undefined) validateFunction(fn, 'onstream');
    this.#onstream = fn;
  }

  // Receives the last-processed stream id when the peer sends an
  // HTTP/3 GOAWAY frame.
  get ongoaway() { return this.#ongoaway; }
  set ongoaway(fn) {
    if (fn !== undefined) validateFunction(fn, 'ongoaway');
    this.#ongoaway = fn;
  }

  // Receives the origin list when the peer sends an HTTP/3 ORIGIN
  // frame (RFC 9412).
  get onorigin() { return this.#onorigin; }
  set onorigin(fn) {
    if (fn === undefined) {
      this.#onorigin = undefined;
      // Tell the application to stop delivering origin announcements.
      getQuicSessionState(this.#session).hasOriginListener = false;
    } else {
      validateFunction(fn, 'onorigin');
      this.#onorigin = fn;
      // Tell the application that origin announcements have a consumer.
      getQuicSessionState(this.#session).hasOriginListener = true;
    }
  }

  // Receives the negotiated HTTP/3 settings when the peer's SETTINGS
  // frame arrives (which may be after the session opens). The current
  // values are also available synchronously via `session.settings`.
  get onsettings() { return this.#onsettings; }
  set onsettings(fn) {
    if (fn !== undefined) validateFunction(fn, 'onsettings');
    this.#onsettings = fn;
  }

  get onerror() { return this.#session.onerror; }
  set onerror(fn) { this.#session.onerror = fn; }


  /** @type {Promise<object>} */
  get opened() { return this.#session.opened; }

  /** @type {Promise<void>} */
  get closed() { return this.#session.closed; }

  /**
   * Opens a request stream: a bidirectional stream carrying the given
   * request headers. If not specified, they can be sent later with
   * stream.sendHeaders() instead.
   *
   * Stream callbacks (onheaders, oninfo, ontrailers, onwanttrailers,
   * onreset, onerror) are passed in options so they are
   * attached before any event can be delivered.
   * @param {object} [headers] the request header block
   * @param {object} [options] stream options (body, callbacks, ...)
   * @returns {Promise<Http3Stream>}
   */
  async request(headers, options = kEmptyObject) {
    if (getQuicSessionState(this.#session).isServer) {
      throw new ERR_INVALID_STATE(
        'Server sessions cannot open HTTP/3 request streams');
    }
    if (headers !== undefined) validateObject(headers, 'headers');
    validateObject(options, 'options');
    // The HTTP/3-level callbacks stay at this layer; only the
    // QUIC-level stream options (body, priority, highWaterMark, ...)
    // are passed down.
    const {
      onheaders,
      oninfo,
      ontrailers,
      onwanttrailers,
      onreset,
      onerror,
      priority,
      incremental,
      ...quicOptions
    } = options;

    let headerString;
    if (headers !== undefined) {
      let toSend = headers;
      const pri = priorityFieldValue(priority ?? 'default', incremental ?? false);
      if (pri !== undefined && !ObjectHasOwn(headers, 'priority')) {
        toSend = { __proto__: null, ...headers, priority: pri };
      }
      headerString = buildNgHeaderString(
        toSend, assertValidPseudoHeader, true /* strictSingleValueFields */);
    }

    const stream = await this.#session.createBidirectionalStream(quicOptions);
    const wrapped = new Http3Stream(stream, this, {
      __proto__: null,
      onheaders,
      oninfo,
      ontrailers,
      onwanttrailers,
      onreset,
      onerror,
    });

    if (priority !== undefined || incremental !== undefined) {
      wrapped.setPriority({ __proto__: null, level: priority, incremental });
    }

    // Submit the request headers only after the callbacks above are
    // attached. Nothing for this stream can hit the wire before the
    // headers are submitted: the HTTP/3 application only learns about the
    // stream (including any body configured at creation) from the
    // header submission itself. When there is no body, the header
    // block also terminates the stream.
    if (headerString !== undefined) {
      const flags = quicOptions.body === undefined ?
        kHeadersFlagsTerminal : kHeadersFlagsNone;
      if (!wrapped[kSubmitInitialHeaders](headerString, flags)) {
        wrapped.destroy();
        throw new ERR_QUIC_OPEN_STREAM_FAILED();
      }
    }
    return wrapped;
  }

  close(options) { return this.#session.close(options); }

  destroy(error, options) { return this.#session.destroy(error, options); }
}

// Register the HTTP/3 application-event callbacks with the binding. The
// C++ layer invokes each with `this` set to the binding handle (stream or
// session) the event belongs to; route it to the wrapper. Events for a
// handle with no live wrapper are dropped. This runs once per realm at
// module load: any HTTP/3 session requires this module, so the callbacks
// are always installed before an event can fire.
setHttp3Callbacks({
  /**
   * A block of headers ([name, value, ...] pairs plus the
   * application's kind constant) was received on a stream.
   * @param {string[]} headers
   * @param {number} kind
   */
  onStreamHeaders(headers, kind) {
    this[kApplicationOwner]?.[kOnHeaders](headers, kind);
  },
  /**
   * The application is ready for a stream's trailing headers.
   */
  onStreamTrailers() {
    this[kApplicationOwner]?.[kOnWantTrailers]();
  },
  /**
   * The peer initiated a graceful shutdown of a session.
   * @param {bigint} lastStreamId
   */
  onSessionGoaway(lastStreamId) {
    this[kApplicationOwner]?.[kOnGoaway](lastStreamId);
  },
  /**
   * The peer announced the origins it claims authority for.
   * @param {string[]} origins
   */
  onSessionOrigin(origins) {
    this[kApplicationOwner]?.[kOnOrigin](origins);
  },
  /**
   * The peer's HTTP/3 SETTINGS were received or updated.
   */
  onSessionApplication() {
    this[kApplicationOwner]?.[kOnSettings]();
  },
});

/**
 * The HTTP/3 settings (RFC 9114 section 7.2 / RFC 9204 SETTINGS values
 * advertised to the peer, plus local header-processing limits).
 * Validated and applied by the HTTP/3 application in C++; the QUIC
 * layer carries them opaquely.
 * @typedef {object} Http3Settings
 * @property {bigint|number} [maxHeaderPairs] Maximum number of header
 *   pairs accepted on a stream (local enforcement limit).
 * @property {bigint|number} [maxHeaderLength] Maximum total header
 *   bytes accepted on a stream (local enforcement limit).
 * @property {bigint|number} [maxFieldSectionSize] The maximum field
 *   section size advertised to the peer.
 * @property {bigint|number} [qpackMaxDTableCapacity] The QPACK maximum
 *   dynamic table capacity.
 * @property {bigint|number} [qpackEncoderMaxDTableCapacity] The QPACK
 *   encoder maximum dynamic table capacity.
 * @property {bigint|number} [qpackBlockedStreams] The maximum number of
 *   QPACK blocked streams.
 * @property {boolean} [enableConnectProtocol] Enable extended CONNECT
 *   (RFC 9220).
 */

/**
 * Connects a built-in QUIC session with ALPN h3 and wraps it. The
 * HTTP/3-level options (settings, ongoaway, onorigin) stay at this
 * layer; the remaining options are passed down to node:quic.
 * @param {SocketAddress|string} address the server address
 * @param {object} [options] http3 + quic connect options
 * @param {Http3Settings} [options.settings] the HTTP/3 settings
 * @returns {Promise<Http3Session>}
 */
async function connect(address, options = kEmptyObject) {
  validateObject(options, 'options');
  const { ongoaway, onorigin, onsettings, settings, ...quicOptions } = options;
  const session = await quicConnect(address, {
    ...quicOptions,
    // The onstream callback is owned by this layer. Datagrams are not (yet)
    // exposed for HTTP/3, so suppress ondatagram too.
    onstream: undefined,
    ondatagram: undefined,
    alpn: kHttp3Alpn,
    [kApplication]: 'http3',
    [kApplicationSettings]: settings,
  });
  return new Http3Session(session, { ongoaway, onorigin, onsettings });
}

/**
 * Listens with ALPN h3; onsession receives an Http3Session, wrapped
 * synchronously in the delivery frame per the attach contract. The
 * HTTP/3-level options (settings, ongoaway, onorigin) stay at this
 * layer; the remaining options are passed down to node:quic.
 * @param {Function} onsession invoked with each new Http3Session
 * @param {object} [options] http3 + quic listen options
 * @param {Http3Settings} [options.settings] the HTTP/3 settings
 * @returns {Promise<object>} the listening QuicEndpoint
 */
async function listen(onsession, options = kEmptyObject) {
  validateFunction(onsession, 'onsession');
  validateObject(options, 'options');
  const { ongoaway, onorigin, onsettings, settings, ...quicOptions } = options;
  return quicListen((session) => {
    return onsession(new Http3Session(session, { ongoaway, onorigin, onsettings }));
  }, {
    ...quicOptions,
    onstream: undefined,
    ondatagram: undefined,
    alpn: kHttp3Alpn,
    [kApplication]: 'http3',
    [kApplicationSettings]: settings,
  });
}

module.exports = {
  Http3Session,
  Http3Stream,
  connect,
  listen,
};
