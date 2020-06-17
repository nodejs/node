'use strict';

const {
  ArrayFrom,
  ArrayIsArray,
  Map,
  ObjectAssign,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectKeys,
  ReflectGetPrototypeOf,
  Set,
  Symbol,
} = primordials;

const {
  customInspectSymbol: kInspect,
} = require('internal/util');
const assert = require('assert');
const EventEmitter = require('events');
const tls = require('tls');
const { URL } = require('url');
const {
  _checkIsHttpToken: checkIsHttpToken
} = require('_http_common');
const JSStreamSocket = require('internal/js_stream_socket');

const {
  symbols: {
    owner_symbol,
  },
} = require('internal/async_hooks');
const {
  codes: {
    ERR_HTTP2_ALTSVC_INVALID_ORIGIN,
    ERR_HTTP2_ALTSVC_LENGTH,
    ERR_HTTP2_CONNECT_AUTHORITY,
    ERR_HTTP2_CONNECT_PATH,
    ERR_HTTP2_CONNECT_SCHEME,
    ERR_HTTP2_GOAWAY_SESSION,
    ERR_HTTP2_INVALID_ORIGIN,
    ERR_HTTP2_INVALID_SESSION,
    ERR_HTTP2_MAX_PENDING_SETTINGS_ACK,
    ERR_HTTP2_NO_SOCKET_MANIPULATION,
    ERR_HTTP2_ORIGIN_LENGTH,
    ERR_HTTP2_OUT_OF_STREAMS,
    ERR_HTTP2_PING_CANCEL,
    ERR_HTTP2_PING_LENGTH,
    ERR_HTTP2_SESSION_ERROR,
    ERR_HTTP2_SETTINGS_CANCEL,
    ERR_HTTP2_SOCKET_BOUND,
    ERR_HTTP2_SOCKET_UNBOUND,
    ERR_HTTP2_STREAM_CANCEL,
    ERR_HTTP2_STREAM_SELF_DEPENDENCY,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_CALLBACK,
    ERR_INVALID_CHAR,
    ERR_INVALID_HTTP_TOKEN,
    ERR_INVALID_OPT_VALUE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const { validateNumber,
        validateString,
        isUint32,
} = require('internal/validators');
const {
  assertIsObject,
  assertValidPseudoHeader,
  callTimeout,
  getSessionState,
  getSettings,
  isPayloadMeaningless,
  sessionName,
  setAndValidatePriorityOptions,
  toHeaderObject,
  kMaxStreams,
  kSocket,
  kState,
  kProxySocket,
  mapToHeaders,
  updateOptionsBuffer,
  updateSettingsBuffer,
  validateSettings,
  NghttpError,
} = require('internal/http2/util');
const {
  debugStream,
  kAuthority,
  kInit,
  kNativeFields,
  kType,
  kProtocol,
  kSentHeaders,
  ClientHttp2Stream,
  ServerHttp2Stream,
  STREAM_FLAGS_HAS_TRAILERS,
  STREAM_FLAGS_HEAD_REQUEST,
} = require('internal/http2/streams');
const {
  kMaybeDestroy,
  kUpdateTimer,
  kHandle,
  kSession,
  setStreamTimeout,
} = require('internal/stream_base_commons');
const { kTimeout } = require('internal/timers');
const { format } = require('internal/util/inspect');
const { isArrayBufferView } = require('internal/util/types');

const binding = internalBinding('http2');

let debug = require('internal/util/debuglog').debuglog('http2', (fn) => {
  debug = fn;
});

function debugSession(sessionType, message, ...args) {
  debug('Http2Session %s: ' + message, sessionName(sessionType), ...args);
}

function debugSessionObj(session, message, ...args) {
  debugSession(session[kType], message, ...args);
}

const kMaxALTSVC = (2 ** 14) - 2;

// eslint-disable-next-line no-control-regex
const kQuotedString = /^[\x09\x20-\x5b\x5d-\x7e\x80-\xff]*$/;

const kAlpnProtocol = Symbol('alpnProtocol');
const kEncrypted = Symbol('encrypted');
const kLocalSettings = Symbol('local-settings');
const kSelectPadding = Symbol('select-padding');
const kPendingRequestCalls = Symbol('kPendingRequestCalls');
const kOwner = owner_symbol;
const kOrigin = Symbol('origin');
const kRemoteSettings = Symbol('remote-settings');
const kServer = Symbol('server');

const {
  constants,
  kBitfield,
  kSessionPriorityListenerCount,
  kSessionFrameErrorListenerCount,
  kSessionMaxInvalidFrames,
  kSessionMaxRejectedStreams,
  kSessionUint8FieldCount,
  kSessionHasRemoteSettingsListeners,
  kSessionRemoteSettingsIsUpToDate,
  kSessionHasPingListeners,
  kSessionHasAltsvcListeners,
} = binding;

const {
  NGHTTP2_REFUSED_STREAM,
  NGHTTP2_FLAG_END_STREAM,
  NGHTTP2_HCAT_PUSH_RESPONSE,
  NGHTTP2_HCAT_RESPONSE,
  NGHTTP2_INTERNAL_ERROR,
  NGHTTP2_NO_ERROR,
  NGHTTP2_SESSION_CLIENT,
  NGHTTP2_SESSION_SERVER,
  NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE,
  NGHTTP2_ERR_INVALID_ARGUMENT,
  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_PROTOCOL,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_STATUS,

  HTTP2_METHOD_GET,
  HTTP2_METHOD_HEAD,
  HTTP2_METHOD_CONNECT,

  HTTP_STATUS_MISDIRECTED_REQUEST,

  STREAM_OPTION_EMPTY_PAYLOAD,
  STREAM_OPTION_GET_TRAILERS
} = constants;

const SESSION_FLAGS_PENDING = 0x0;
const SESSION_FLAGS_READY = 0x1;
const SESSION_FLAGS_CLOSED = 0x2;
const SESSION_FLAGS_DESTROYED = 0x4;

// Top level to avoid creating a closure
function emit(self, ...args) {
  self.emit(...args);
}

// Upon creation, the Http2Session takes ownership of the socket. The session
// may not be ready to use immediately if the socket is not yet fully connected.
// In that case, the Http2Session will wait for the socket to connect. Once
// the Http2Session is ready, it will emit its own 'connect' event.
//
// The Http2Session.goaway() method will send a GOAWAY frame, signalling
// to the connected peer that a shutdown is in progress. Sending a goaway
// frame has no other effect, however.
//
// Receiving a GOAWAY frame will cause the Http2Session to first emit a 'goaway'
// event notifying the user that a shutdown is in progress. If the goaway
// error code equals 0 (NGHTTP2_NO_ERROR), session.close() will be called,
// causing the Http2Session to send its own GOAWAY frame and switch itself
// into a graceful closing state. In this state, new inbound or outbound
// Http2Streams will be rejected. Existing *pending* streams (those created
// but without an assigned stream ID or handle) will be destroyed with a
// cancel error. Existing open streams will be permitted to complete on their
// own. Once all existing streams close, session.destroy() will be called
// automatically.
//
// Calling session.destroy() will tear down the Http2Session immediately,
// making it no longer usable. Pending and existing streams will be destroyed.
// The bound socket will be destroyed. Once all resources have been freed up,
// the 'close' event will be emitted. Note that pending streams will be
// destroyed using a specific "ERR_HTTP2_STREAM_CANCEL" error. Existing open
// streams will be destroyed using the same error passed to session.destroy()
//
// If destroy is called with an error, an 'error' event will be emitted
// immediately following the 'close' event.
//
// The socket and Http2Session lifecycles are tightly bound. Once one is
// destroyed, the other should also be destroyed. When the socket is destroyed
// with an error, session.destroy() will be called with that same error.
// Likewise, when session.destroy() is called with an error, the same error
// will be sent to the socket.
class Http2Session extends EventEmitter {
  constructor(type, options, socket) {
    super();

    if (!socket._handle || !socket._handle.isStreamBase) {
      socket = new JSStreamSocket(socket);
    }

    // No validation is performed on the input parameters because this
    // constructor is not exported directly for users.

    // If the session property already exists on the socket,
    // then it has already been bound to an Http2Session instance
    // and cannot be attached again.
    if (socket[kSession] !== undefined)
      throw new ERR_HTTP2_SOCKET_BOUND();

    socket[kSession] = this;

    this[kState] = {
      destroyCode: NGHTTP2_NO_ERROR,
      flags: SESSION_FLAGS_PENDING,
      goawayCode: null,
      goawayLastStreamID: null,
      streams: new Map(),
      pendingStreams: new Set(),
      pendingAck: 0,
      writeQueueSize: 0,
      originSet: undefined
    };

    this[kEncrypted] = undefined;
    this[kAlpnProtocol] = undefined;
    this[kType] = type;
    this[kProxySocket] = null;
    this[kSocket] = socket;
    this[kTimeout] = null;
    this[kHandle] = undefined;

    // Do not use nagle's algorithm
    if (typeof socket.setNoDelay === 'function')
      socket.setNoDelay();

    // Disable TLS renegotiation on the socket
    if (typeof socket.disableRenegotiation === 'function')
      socket.disableRenegotiation();

    const setupFn = setupHandle.bind(this, socket, type, options);
    if (socket.connecting || socket.secureConnecting) {
      const connectEvent =
        socket instanceof tls.TLSSocket ? 'secureConnect' : 'connect';
      socket.once(connectEvent, () => {
        try {
          setupFn();
        } catch (error) {
          socket.destroy(error);
        }
      });
    } else {
      setupFn();
    }

    if (!this[kNativeFields])
      this[kNativeFields] = new Uint8Array(kSessionUint8FieldCount);
    this.on('newListener', sessionListenerAdded);
    this.on('removeListener', sessionListenerRemoved);

    debugSession(type, 'created');
  }

  // Returns undefined if the socket is not yet connected, true if the
  // socket is a TLSSocket, and false if it is not.
  get encrypted() {
    return this[kEncrypted];
  }

  // Returns undefined if the socket is not yet connected, `h2` if the
  // socket is a TLSSocket and the alpnProtocol is `h2`, or `h2c` if the
  // socket is not a TLSSocket.
  get alpnProtocol() {
    return this[kAlpnProtocol];
  }

  // TODO(jasnell): originSet is being added in preparation for ORIGIN frame
  // support. At the current time, the ORIGIN frame specification is awaiting
  // publication as an RFC and is awaiting implementation in nghttp2. Once
  // added, an ORIGIN frame will add to the origins included in the origin
  // set. 421 responses will remove origins from the set.
  get originSet() {
    if (!this.encrypted || this.destroyed)
      return undefined;
    return ArrayFrom(initOriginSet(this));
  }

  // True if the Http2Session is still waiting for the socket to connect
  get connecting() {
    return (this[kState].flags & SESSION_FLAGS_READY) === 0;
  }

  // True if Http2Session.prototype.close() has been called
  get closed() {
    return !!(this[kState].flags & SESSION_FLAGS_CLOSED);
  }

  // True if Http2Session.prototype.destroy() has been called
  get destroyed() {
    return !!(this[kState].flags & SESSION_FLAGS_DESTROYED);
  }

  // Resets the timeout counter
  [kUpdateTimer]() {
    if (this.destroyed)
      return;
    if (this[kTimeout]) this[kTimeout].refresh();
  }

  // Sets the id of the next stream to be created by this Http2Session.
  // The value must be a number in the range 0 <= n <= kMaxStreams. The
  // value also needs to be larger than the current next stream ID.
  setNextStreamID(id) {
    if (this.destroyed)
      throw new ERR_HTTP2_INVALID_SESSION();

    validateNumber(id, 'id');
    if (id <= 0 || id > kMaxStreams)
      throw new ERR_OUT_OF_RANGE('id', `> 0 and <= ${kMaxStreams}`, id);
    this[kHandle].setNextStreamID(id);
  }

  // If ping is called while we are still connecting, or after close() has
  // been called, the ping callback will be invoked immediately will a ping
  // cancelled error and a duration of 0.0.
  ping(payload, callback) {
    if (this.destroyed)
      throw new ERR_HTTP2_INVALID_SESSION();

    if (typeof payload === 'function') {
      callback = payload;
      payload = undefined;
    }
    if (payload && !isArrayBufferView(payload)) {
      throw new ERR_INVALID_ARG_TYPE('payload',
                                     ['Buffer', 'TypedArray', 'DataView'],
                                     payload);
    }
    if (payload && payload.length !== 8) {
      throw new ERR_HTTP2_PING_LENGTH();
    }
    if (typeof callback !== 'function')
      throw new ERR_INVALID_CALLBACK(callback);

    const cb = pingCallback(callback);
    if (this.connecting || this.closed) {
      process.nextTick(cb, false, 0.0, payload);
      return;
    }

    return this[kHandle].ping(payload, cb);
  }

  [kInspect](depth, opts) {
    if (typeof depth === 'number' && depth < 0)
      return this;

    const obj = {
      type: this[kType],
      closed: this.closed,
      destroyed: this.destroyed,
      state: this.state,
      localSettings: this.localSettings,
      remoteSettings: this.remoteSettings
    };
    return `Http2Session ${format(obj)}`;
  }

  // The socket owned by this session
  get socket() {
    const proxySocket = this[kProxySocket];
    if (proxySocket === null)
      return this[kProxySocket] = new Proxy(this, proxySocketHandler);
    return proxySocket;
  }

  // The session type
  get type() {
    return this[kType];
  }

  // If a GOAWAY frame has been received, gives the error code specified
  get goawayCode() {
    return this[kState].goawayCode || NGHTTP2_NO_ERROR;
  }

  // If a GOAWAY frame has been received, gives the last stream ID reported
  get goawayLastStreamID() {
    return this[kState].goawayLastStreamID || 0;
  }

  // True if the Http2Session is waiting for a settings acknowledgement
  get pendingSettingsAck() {
    return this[kState].pendingAck > 0;
  }

  // Retrieves state information for the Http2Session
  get state() {
    return this.connecting || this.destroyed ?
      {} : getSessionState(this[kHandle]);
  }

  // The settings currently in effect for the local peer. These will
  // be updated only when a settings acknowledgement has been received.
  get localSettings() {
    const settings = this[kLocalSettings];
    if (settings !== undefined)
      return settings;

    if (this.destroyed || this.connecting)
      return {};

    return this[kLocalSettings] = getSettings(this[kHandle], false); // Local
  }

  // The settings currently in effect for the remote peer.
  get remoteSettings() {
    if (this[kNativeFields][kBitfield] &
        (1 << kSessionRemoteSettingsIsUpToDate)) {
      const settings = this[kRemoteSettings];
      if (settings !== undefined) {
        return settings;
      }
    }

    if (this.destroyed || this.connecting)
      return {};

    this[kNativeFields][kBitfield] |= (1 << kSessionRemoteSettingsIsUpToDate);
    return this[kRemoteSettings] = getSettings(this[kHandle], true); // Remote
  }

  // Submits a SETTINGS frame to be sent to the remote peer.
  settings(settings, callback) {
    if (this.destroyed)
      throw new ERR_HTTP2_INVALID_SESSION();
    assertIsObject(settings, 'settings');
    validateSettings(settings);

    if (callback && typeof callback !== 'function')
      throw new ERR_INVALID_CALLBACK(callback);
    debugSessionObj(this, 'sending settings');

    this[kState].pendingAck++;

    const settingsFn = submitSettings.bind(this, { ...settings }, callback);
    if (this.connecting) {
      this.once('connect', settingsFn);
      return;
    }
    settingsFn();
  }

  // Sumits a GOAWAY frame to be sent to the remote peer. Note that this
  // is only a notification, and does not affect the usable state of the
  // session with the notable exception that new incoming streams will
  // be rejected automatically.
  goaway(code = NGHTTP2_NO_ERROR, lastStreamID = 0, opaqueData) {
    if (this.destroyed)
      throw new ERR_HTTP2_INVALID_SESSION();

    if (opaqueData !== undefined && !isArrayBufferView(opaqueData)) {
      throw new ERR_INVALID_ARG_TYPE('opaqueData',
                                     ['Buffer', 'TypedArray', 'DataView'],
                                     opaqueData);
    }
    validateNumber(code, 'code');
    validateNumber(lastStreamID, 'lastStreamID');

    const goawayFn = submitGoaway.bind(this, code, lastStreamID, opaqueData);
    if (this.connecting) {
      this.once('connect', goawayFn);
      return;
    }
    goawayFn();
  }

  // Destroy the Http2Session, making it no longer usable and cancelling
  // any pending activity.
  destroy(error = NGHTTP2_NO_ERROR, code) {
    if (this.destroyed)
      return;

    debugSessionObj(this, 'destroying');

    if (typeof error === 'number') {
      code = error;
      error =
        code !== NGHTTP2_NO_ERROR ?
          new ERR_HTTP2_SESSION_ERROR(code) : undefined;
    }
    if (code === undefined && error != null)
      code = NGHTTP2_INTERNAL_ERROR;

    closeSession(this, code, error);
  }

  // Closing the session will:
  // 1. Send a goaway frame
  // 2. Mark the session as closed
  // 3. Prevent new inbound or outbound streams from being opened
  // 4. Optionally register a 'close' event handler
  // 5. Will cause the session to automatically destroy after the
  //    last currently open Http2Stream closes.
  //
  // Close always assumes a good, non-error shutdown (NGHTTP_NO_ERROR)
  //
  // If the session has not connected yet, the closed flag will still be
  // set but the goaway will not be sent until after the connect event
  // is emitted.
  close(callback) {
    if (this.closed || this.destroyed)
      return;
    debugSessionObj(this, 'marking session closed');
    this[kState].flags |= SESSION_FLAGS_CLOSED;
    if (typeof callback === 'function')
      this.once('close', callback);
    this.goaway();
    this[kMaybeDestroy]();
  }

  [EventEmitter.captureRejectionSymbol](err, event, ...args) {
    switch (event) {
      case 'stream':
        const [stream] = args;
        stream.destroy(err);
        break;
      default:
        this.destroy(err);
    }
  }

  // Destroy the session if:
  // * error is not undefined/null
  // * session is closed and there are no more pending or open streams
  [kMaybeDestroy](error) {
    if (error == null) {
      const state = this[kState];
      // Do not destroy if we're not closed and there are pending/open streams
      if (!this.closed ||
          state.streams.size > 0 ||
          state.pendingStreams.size > 0) {
        return;
      }
    }
    this.destroy(error);
  }

  _onTimeout() {
    callTimeout(this);
  }

  ref() {
    if (this[kSocket]) {
      this[kSocket].ref();
    }
  }

  unref() {
    if (this[kSocket]) {
      this[kSocket].unref();
    }
  }
}

// ServerHttp2Session instances should never have to wait for the socket
// to connect as they are always created after the socket has already been
// established.
class ServerHttp2Session extends Http2Session {
  constructor(options, socket, server) {
    super(NGHTTP2_SESSION_SERVER, options, socket);
    this[kServer] = server;
    // This is a bit inaccurate because it does not reflect changes to
    // number of listeners made after the session was created. This should
    // not be an issue in practice. Additionally, the 'priority' event on
    // server instances (or any other object) is fully undocumented.
    this[kNativeFields][kSessionPriorityListenerCount] =
      server.listenerCount('priority');
  }

  get server() {
    return this[kServer];
  }

  // Submits an altsvc frame to be sent to the client. `stream` is a
  // numeric Stream ID. origin is a URL string that will be used to get
  // the origin. alt is a string containing the altsvc details. No fancy
  // API is provided for that.
  altsvc(alt, originOrStream) {
    if (this.destroyed)
      throw new ERR_HTTP2_INVALID_SESSION();

    let stream = 0;
    let origin;

    if (typeof originOrStream === 'string') {
      origin = (new URL(originOrStream)).origin;
      if (origin === 'null')
        throw new ERR_HTTP2_ALTSVC_INVALID_ORIGIN();
    } else if (typeof originOrStream === 'number') {
      if (originOrStream >>> 0 !== originOrStream || originOrStream === 0) {
        throw new ERR_OUT_OF_RANGE('originOrStream',
                                   `> 0 && < ${2 ** 32}`, originOrStream);
      }
      stream = originOrStream;
    } else if (originOrStream !== undefined) {
      // Allow origin to be passed a URL or object with origin property
      if (originOrStream !== null && typeof originOrStream === 'object')
        origin = originOrStream.origin;
      // Note: if originOrStream is an object with an origin property other
      // than a URL, then it is possible that origin will be malformed.
      // We do not verify that here. Users who go that route need to
      // ensure they are doing the right thing or the payload data will
      // be invalid.
      if (typeof origin !== 'string') {
        throw new ERR_INVALID_ARG_TYPE('originOrStream',
                                       ['string', 'number', 'URL', 'object'],
                                       originOrStream);
      } else if (origin === 'null' || origin.length === 0) {
        throw new ERR_HTTP2_ALTSVC_INVALID_ORIGIN();
      }
    }

    validateString(alt, 'alt');
    if (!kQuotedString.test(alt))
      throw new ERR_INVALID_CHAR('alt');

    // Max length permitted for ALTSVC
    if ((alt.length + (origin !== undefined ? origin.length : 0)) > kMaxALTSVC)
      throw new ERR_HTTP2_ALTSVC_LENGTH();

    this[kHandle].altsvc(stream, origin || '', alt);
  }

  // Submits an origin frame to be sent.
  origin(...origins) {
    if (this.destroyed)
      throw new ERR_HTTP2_INVALID_SESSION();

    if (origins.length === 0)
      return;

    let arr = '';
    let len = 0;
    const count = origins.length;
    for (let i = 0; i < count; i++) {
      let origin = origins[i];
      if (typeof origin === 'string') {
        origin = (new URL(origin)).origin;
      } else if (origin != null && typeof origin === 'object') {
        origin = origin.origin;
      }
      validateString(origin, 'origin');
      if (origin === 'null')
        throw new ERR_HTTP2_INVALID_ORIGIN();

      arr += `${origin}\0`;
      len += origin.length;
    }

    if (len > kMaxALTSVC)
      throw new ERR_HTTP2_ORIGIN_LENGTH();

    this[kHandle].origin(arr, count);
  }

}

// ClientHttp2Session instances have to wait for the socket to connect after
// they have been created. Various operations such as request() may be used,
// but the actual protocol communication will only occur after the socket
// has been connected.
class ClientHttp2Session extends Http2Session {
  constructor(options, socket) {
    super(NGHTTP2_SESSION_CLIENT, options, socket);
    this[kPendingRequestCalls] = null;
  }

  // Submits a new HTTP2 request to the connected peer. Returns the
  // associated Http2Stream instance.
  request(headers, options) {
    debugSessionObj(this, 'initiating request');

    if (this.destroyed)
      throw new ERR_HTTP2_INVALID_SESSION();

    if (this.closed)
      throw new ERR_HTTP2_GOAWAY_SESSION();

    this[kUpdateTimer]();

    if (headers !== null && headers !== undefined) {
      for (const header of ObjectKeys(headers)) {
        if (header[0] === ':') {
          assertValidPseudoHeader(header);
        } else if (header && !checkIsHttpToken(header))
          this.destroy(new ERR_INVALID_HTTP_TOKEN('Header name', header));
      }
    }

    assertIsObject(headers, 'headers');
    assertIsObject(options, 'options');

    headers = ObjectAssign(ObjectCreate(null), headers);
    options = { ...options };

    if (headers[HTTP2_HEADER_METHOD] === undefined)
      headers[HTTP2_HEADER_METHOD] = HTTP2_METHOD_GET;

    const connect = headers[HTTP2_HEADER_METHOD] === HTTP2_METHOD_CONNECT;

    if (!connect || headers[HTTP2_HEADER_PROTOCOL] !== undefined) {
      if (headers[HTTP2_HEADER_AUTHORITY] === undefined)
        headers[HTTP2_HEADER_AUTHORITY] = this[kAuthority];
      if (headers[HTTP2_HEADER_SCHEME] === undefined)
        headers[HTTP2_HEADER_SCHEME] = this[kProtocol].slice(0, -1);
      if (headers[HTTP2_HEADER_PATH] === undefined)
        headers[HTTP2_HEADER_PATH] = '/';
    } else {
      if (headers[HTTP2_HEADER_AUTHORITY] === undefined)
        throw new ERR_HTTP2_CONNECT_AUTHORITY();
      if (headers[HTTP2_HEADER_SCHEME] !== undefined)
        throw new ERR_HTTP2_CONNECT_SCHEME();
      if (headers[HTTP2_HEADER_PATH] !== undefined)
        throw new ERR_HTTP2_CONNECT_PATH();
    }

    setAndValidatePriorityOptions(options);

    if (options.endStream === undefined) {
      // For some methods, we know that a payload is meaningless, so end the
      // stream by default if the user has not specifically indicated a
      // preference.
      options.endStream = isPayloadMeaningless(headers[HTTP2_HEADER_METHOD]);
    } else if (typeof options.endStream !== 'boolean') {
      throw new ERR_INVALID_OPT_VALUE('endStream', options.endStream);
    }

    const headersList = mapToHeaders(headers);

    const stream = new ClientHttp2Stream(this, undefined, undefined, {});
    stream[kSentHeaders] = headers;
    stream[kOrigin] = `${headers[HTTP2_HEADER_SCHEME]}://` +
                      `${headers[HTTP2_HEADER_AUTHORITY]}`;

    // Close the writable side of the stream if options.endStream is set.
    if (options.endStream)
      stream.end();

    if (options.waitForTrailers)
      stream[kState].flags |= STREAM_FLAGS_HAS_TRAILERS;

    const onConnect = requestOnConnect.bind(stream, headersList, options);
    if (this.connecting) {
      if (this[kPendingRequestCalls] !== null) {
        this[kPendingRequestCalls].push(onConnect);
      } else {
        this[kPendingRequestCalls] = [onConnect];
        this.once('connect', () => {
          this[kPendingRequestCalls].forEach((f) => f());
          this[kPendingRequestCalls] = null;
        });
      }
    } else {
      onConnect();
    }
    return stream;
  }
}

ObjectDefineProperty(Http2Session.prototype, 'setTimeout', {
  configurable: true,
  enumerable: true,
  writable: true,
  value: setStreamTimeout
});

// Called when a new block of headers has been received for a given
// stream. The stream may or may not be new. If the stream is new,
// create the associated Http2Stream instance and emit the 'stream'
// event. If the stream is not new, emit the 'headers' event to pass
// the block of headers on.
function onSessionHeaders(handle, id, cat, flags, headers, sensitiveHeaders) {
  const session = this[kOwner];
  if (session.destroyed)
    return;

  const type = session[kType];
  session[kUpdateTimer]();
  debugStream(id, type, 'headers received');
  const streams = session[kState].streams;

  const endOfStream = !!(flags & NGHTTP2_FLAG_END_STREAM);
  let stream = streams.get(id);

  // Convert the array of header name value pairs into an object
  const obj = toHeaderObject(headers, sensitiveHeaders);

  if (stream === undefined) {
    if (session.closed) {
      // We are not accepting any new streams at this point. This callback
      // should not be invoked at this point in time, but just in case it is,
      // refuse the stream using an RST_STREAM and destroy the handle.
      handle.rstStream(NGHTTP2_REFUSED_STREAM);
      handle.destroy();
      return;
    }
    const opts = { readable: !endOfStream };
    // session[kType] can be only one of two possible values
    if (type === NGHTTP2_SESSION_SERVER) {
      stream = new ServerHttp2Stream(session, handle, id, opts, obj);
      if (obj[HTTP2_HEADER_METHOD] === HTTP2_METHOD_HEAD) {
        // For head requests, there must not be a body...
        // end the writable side immediately.
        stream.end();
        stream[kState].flags |= STREAM_FLAGS_HEAD_REQUEST;
      }
    } else {
      stream = new ClientHttp2Stream(session, handle, id, opts);
      stream.end();
    }
    if (endOfStream)
      stream[kState].endAfterHeaders = true;
    process.nextTick(emit, session, 'stream', stream, obj, flags, headers);
  } else {
    let event;
    const status = obj[HTTP2_HEADER_STATUS];
    if (cat === NGHTTP2_HCAT_RESPONSE) {
      if (!endOfStream &&
          status !== undefined &&
          status >= 100 &&
          status < 200) {
        event = 'headers';
      } else {
        event = 'response';
      }
    } else if (cat === NGHTTP2_HCAT_PUSH_RESPONSE) {
      event = 'push';
      // cat === NGHTTP2_HCAT_HEADERS:
    } else if (!endOfStream && status !== undefined && status >= 200) {
      event = 'response';
    } else {
      event = endOfStream ? 'trailers' : 'headers';
    }
    const session = stream.session;
    if (status === HTTP_STATUS_MISDIRECTED_REQUEST) {
      const originSet = session[kState].originSet = initOriginSet(session);
      originSet.delete(stream[kOrigin]);
    }
    debugStream(id, type, "emitting stream '%s' event", event);
    process.nextTick(emit, stream, event, obj, flags, headers);
  }
  if (endOfStream) {
    stream.push(null);
  }
}

function initOriginSet(session) {
  let originSet = session[kState].originSet;
  if (originSet === undefined) {
    const socket = session[kSocket];
    session[kState].originSet = originSet = new Set();
    if (socket.servername != null) {
      let originString = `https://${socket.servername}`;
      if (socket.remotePort != null)
        originString += `:${socket.remotePort}`;
      // We have to ensure that it is a properly serialized
      // ASCII origin string. The socket.servername might not
      // be properly ASCII encoded.
      originSet.add((new URL(originString)).origin);
    }
  }
  return originSet;
}

// Submit a GOAWAY frame to be sent to the remote peer.
// If the lastStreamID is set to <= 0, then the lastProcStreamID will
// be used. The opaqueData must either be a typed array or undefined
// (which will be checked elsewhere).
function submitGoaway(code, lastStreamID, opaqueData) {
  if (this.destroyed)
    return;
  debugSessionObj(this, 'submitting goaway');
  this[kUpdateTimer]();
  this[kHandle].goaway(code, lastStreamID, opaqueData);
}

const proxySocketHandler = {
  get(session, prop) {
    switch (prop) {
      case 'setTimeout':
      case 'ref':
      case 'unref':
        return session[prop].bind(session);
      case 'destroy':
      case 'emit':
      case 'end':
      case 'pause':
      case 'read':
      case 'resume':
      case 'write':
      case 'setEncoding':
      case 'setKeepAlive':
      case 'setNoDelay':
        throw new ERR_HTTP2_NO_SOCKET_MANIPULATION();
      default:
        const socket = session[kSocket];
        if (socket === undefined)
          throw new ERR_HTTP2_SOCKET_UNBOUND();
        const value = socket[prop];
        return typeof value === 'function' ? value.bind(socket) : value;
    }
  },
  getPrototypeOf(session) {
    const socket = session[kSocket];
    if (socket === undefined)
      throw new ERR_HTTP2_SOCKET_UNBOUND();
    return ReflectGetPrototypeOf(socket);
  },
  set(session, prop, value) {
    switch (prop) {
      case 'setTimeout':
      case 'ref':
      case 'unref':
        session[prop] = value;
        return true;
      case 'destroy':
      case 'emit':
      case 'end':
      case 'pause':
      case 'read':
      case 'resume':
      case 'write':
      case 'setEncoding':
      case 'setKeepAlive':
      case 'setNoDelay':
        throw new ERR_HTTP2_NO_SOCKET_MANIPULATION();
      default:
        const socket = session[kSocket];
        if (socket === undefined)
          throw new ERR_HTTP2_SOCKET_UNBOUND();
        socket[prop] = value;
        return true;
    }
  }
};

// Emits a close event followed by an error event if err is truthy. Used
// by Http2Session.prototype.destroy()
function emitClose(self, error) {
  if (error)
    self.emit('error', error);
  self.emit('close');
}

function cleanupSession(session) {
  const socket = session[kSocket];
  const handle = session[kHandle];
  session[kProxySocket] = undefined;
  session[kSocket] = undefined;
  session[kHandle] = undefined;
  session[kNativeFields] = new Uint8Array(kSessionUint8FieldCount);
  if (handle)
    handle.ondone = null;
  if (socket) {
    socket[kSession] = undefined;
    socket[kServer] = undefined;
  }
}

function finishSessionClose(session, error) {
  debugSessionObj(session, 'finishSessionClose');

  const socket = session[kSocket];
  cleanupSession(session);

  if (socket && !socket.destroyed) {
    // Always wait for writable side to finish.
    socket.end((err) => {
      debugSessionObj(session, 'finishSessionClose socket end', err);
      // Due to the way the underlying stream is handled in Http2Session we
      // won't get graceful Readable end from the other side even if it was sent
      // as the stream is already considered closed and will neither be read
      // from nor keep the event loop alive.
      // Therefore destroy the socket immediately.
      // Fixing this would require some heavy juggling of ReadStart/ReadStop
      // mostly on Windows as on Unix it will be fine with just ReadStart
      // after this 'ondone' callback.
      socket.destroy(error);
      emitClose(session, error);
    });
  } else {
    process.nextTick(emitClose, session, error);
  }
}

function closeSession(session, code, error) {
  debugSessionObj(session, 'start closing/destroying');

  const state = session[kState];
  state.flags |= SESSION_FLAGS_DESTROYED;
  state.destroyCode = code;

  // Clear timeout and remove timeout listeners.
  session.setTimeout(0);
  session.removeAllListeners('timeout');

  // Destroy any pending and open streams
  if (state.pendingStreams.size > 0 || state.streams.size > 0) {
    const cancel = new ERR_HTTP2_STREAM_CANCEL(error);
    state.pendingStreams.forEach((stream) => stream.destroy(cancel));
    state.streams.forEach((stream) => stream.destroy(error));
  }

  // Disassociate from the socket and server.
  const socket = session[kSocket];
  const handle = session[kHandle];

  // Destroy the handle if it exists at this point.
  if (handle !== undefined) {
    handle.ondone = finishSessionClose.bind(null, session, error);
    handle.destroy(code, socket.destroyed);
  } else {
    finishSessionClose(session, error);
  }
}

// Keep track of the number/presence of JS event listeners. Knowing that there
// are no listeners allows the C++ code to skip calling into JS for an event.
function sessionListenerAdded(name) {
  switch (name) {
    case 'ping':
      this[kNativeFields][kBitfield] |= 1 << kSessionHasPingListeners;
      break;
    case 'altsvc':
      this[kNativeFields][kBitfield] |= 1 << kSessionHasAltsvcListeners;
      break;
    case 'remoteSettings':
      this[kNativeFields][kBitfield] |= 1 << kSessionHasRemoteSettingsListeners;
      break;
    case 'priority':
      this[kNativeFields][kSessionPriorityListenerCount]++;
      break;
    case 'frameError':
      this[kNativeFields][kSessionFrameErrorListenerCount]++;
      break;
  }
}

function sessionListenerRemoved(name) {
  switch (name) {
    case 'ping':
      if (this.listenerCount(name) > 0) return;
      this[kNativeFields][kBitfield] &= ~(1 << kSessionHasPingListeners);
      break;
    case 'altsvc':
      if (this.listenerCount(name) > 0) return;
      this[kNativeFields][kBitfield] &= ~(1 << kSessionHasAltsvcListeners);
      break;
    case 'remoteSettings':
      if (this.listenerCount(name) > 0) return;
      this[kNativeFields][kBitfield] &=
          ~(1 << kSessionHasRemoteSettingsListeners);
      break;
    case 'priority':
      this[kNativeFields][kSessionPriorityListenerCount]--;
      break;
    case 'frameError':
      this[kNativeFields][kSessionFrameErrorListenerCount]--;
      break;
  }
}

// When a ClientHttp2Session is first created, the socket may not yet be
// connected. If request() is called during this time, the actual request
// will be deferred until the socket is ready to go.
function requestOnConnect(headers, options) {
  const session = this[kSession];

  // At this point, the stream should have already been destroyed during
  // the session.destroy() method. Do nothing else.
  if (session === undefined || session.destroyed)
    return;

  // If the session was closed while waiting for the connect, destroy
  // the stream and do not continue with the request.
  if (session.closed) {
    const err = new ERR_HTTP2_GOAWAY_SESSION();
    this.destroy(err);
    return;
  }

  debugSessionObj(session, 'connected, initializing request');

  let streamOptions = 0;
  if (options.endStream)
    streamOptions |= STREAM_OPTION_EMPTY_PAYLOAD;

  if (options.waitForTrailers)
    streamOptions |= STREAM_OPTION_GET_TRAILERS;

  // `ret` will be either the reserved stream ID (if positive)
  // or an error code (if negative)
  const ret = session[kHandle].request(headers,
                                       streamOptions,
                                       options.parent | 0,
                                       options.weight | 0,
                                       !!options.exclusive);

  // In an error condition, one of three possible response codes will be
  // possible:
  // * NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE - Maximum stream ID is reached, this
  //   is fatal for the session
  // * NGHTTP2_ERR_INVALID_ARGUMENT - Stream was made dependent on itself, this
  //   impacts on this stream.
  // For the first two, emit the error on the session,
  // For the third, emit the error on the stream, it will bubble up to the
  // session if not handled.
  if (typeof ret === 'number') {
    let err;
    switch (ret) {
      case NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE:
        err = new ERR_HTTP2_OUT_OF_STREAMS();
        this.destroy(err);
        break;
      case NGHTTP2_ERR_INVALID_ARGUMENT:
        err = new ERR_HTTP2_STREAM_SELF_DEPENDENCY();
        this.destroy(err);
        break;
      default:
        session.destroy(new NghttpError(ret));
    }
    return;
  }
  this[kInit](ret.id(), ret);
}

function settingsCallback(cb, ack, duration) {
  this[kState].pendingAck--;
  this[kLocalSettings] = undefined;
  if (ack) {
    debugSessionObj(this, 'settings received');
    const settings = this.localSettings;
    if (typeof cb === 'function')
      cb(null, settings, duration);
    this.emit('localSettings', settings);
  } else {
    debugSessionObj(this, 'settings canceled');
    if (typeof cb === 'function')
      cb(new ERR_HTTP2_SETTINGS_CANCEL());
  }
}

// Submits a SETTINGS frame to be sent to the remote peer.
function submitSettings(settings, callback) {
  if (this.destroyed)
    return;
  debugSessionObj(this, 'submitting settings');
  this[kUpdateTimer]();
  updateSettingsBuffer(settings);
  if (!this[kHandle].settings(settingsCallback.bind(this, callback))) {
    this.destroy(new ERR_HTTP2_MAX_PENDING_SETTINGS_ACK());
  }
}

// pingCallback() returns a function that is invoked when an HTTP2 PING
// frame acknowledgement is received. The ack is either true or false to
// indicate if the ping was successful or not. The duration indicates the
// number of milliseconds elapsed since the ping was sent and the ack
// received. The payload is a Buffer containing the 8 bytes of payload
// data received on the PING acknowledgement.
function pingCallback(cb) {
  return function pingCallback(ack, duration, payload) {
    if (ack) {
      cb(null, duration, payload);
    } else {
      cb(new ERR_HTTP2_PING_CANCEL());
    }
  };
}

// Creates the internal binding.Http2Session handle for an Http2Session
// instance. This occurs only after the socket connection has been
// established. Note: the binding.Http2Session will take over ownership
// of the socket. No other code should read from or write to the socket.
function setupHandle(socket, type, options) {
  // If the session has been destroyed, go ahead and emit 'connect',
  // but do nothing else. The various on('connect') handlers set by
  // core will check for session.destroyed before progressing, this
  // ensures that those at l`east get cleared out.
  if (this.destroyed) {
    process.nextTick(emit, this, 'connect', this, socket);
    return;
  }

  assert(socket._handle !== undefined,
         'Internal HTTP/2 Failure. The socket is not connected. Please ' +
         'report this as a bug in Node.js');

  debugSession(type, 'setting up session handle');
  this[kState].flags |= SESSION_FLAGS_READY;

  updateOptionsBuffer(options);
  const handle = new binding.Http2Session(type);
  handle[kOwner] = this;

  if (typeof options.selectPadding === 'function')
    this[kSelectPadding] = options.selectPadding;
  handle.consume(socket._handle);

  this[kHandle] = handle;
  if (this[kNativeFields])
    handle.fields.set(this[kNativeFields]);
  else
    this[kNativeFields] = handle.fields;

  if (socket.encrypted) {
    this[kAlpnProtocol] = socket.alpnProtocol;
    this[kEncrypted] = true;
  } else {
    // 'h2c' is the protocol identifier for HTTP/2 over plain-text. We use
    // it here to identify any session that is not explicitly using an
    // encrypted socket.
    this[kAlpnProtocol] = 'h2c';
    this[kEncrypted] = false;
  }

  if (isUint32(options.maxSessionInvalidFrames)) {
    const uint32 = new Uint32Array(
      this[kNativeFields].buffer, kSessionMaxInvalidFrames, 1);
    uint32[0] = options.maxSessionInvalidFrames;
  }

  if (isUint32(options.maxSessionRejectedStreams)) {
    const uint32 = new Uint32Array(
      this[kNativeFields].buffer, kSessionMaxRejectedStreams, 1);
    uint32[0] = options.maxSessionRejectedStreams;
  }

  const settings = typeof options.settings === 'object' ?
    options.settings : {};

  this.settings(settings);

  if (type === NGHTTP2_SESSION_SERVER &&
      ArrayIsArray(options.origins)) {
    this.origin(...options.origins);
  }

  process.nextTick(emit, this, 'connect', this, socket);
}

// Handles the on('stream') event for a session and forwards
// it on to the server object.
function sessionOnStream(stream, headers, flags, rawHeaders) {
  if (this[kServer] !== undefined)
    this[kServer].emit('stream', stream, headers, flags, rawHeaders);
}

function sessionOnPriority(stream, parent, weight, exclusive) {
  if (this[kServer] !== undefined)
    this[kServer].emit('priority', stream, parent, weight, exclusive);
}

function sessionOnError(error) {
  if (this[kServer] !== undefined)
    this[kServer].emit('sessionError', error, this);
}

// When the session times out on the server, try emitting a timeout event.
// If no handler is registered, destroy the session.
function sessionOnTimeout() {
  // If destroyed or closed already, do nothing
  if (this.destroyed || this.closed)
    return;
  const server = this[kServer];
  if (!server.emit('timeout', this))
    this.destroy();  // No error code, just things down.
}

// When an error occurs internally at the binding level, immediately
// destroy the session.
function onSessionInternalError(code) {
  if (this[kOwner] !== undefined)
    this[kOwner].destroy(new NghttpError(code));
}

function onPing(payload) {
  const session = this[kOwner];
  if (session.destroyed)
    return;
  session[kUpdateTimer]();
  debugSessionObj(session, 'new ping received');
  session.emit('ping', payload);
}

// Called when the remote peer settings have been updated.
// Resets the cached settings.
function onSettings() {
  const session = this[kOwner];
  if (session.destroyed)
    return;
  session[kUpdateTimer]();
  debugSessionObj(session, 'new settings received');
  session[kRemoteSettings] = undefined;
  session.emit('remoteSettings', session.remoteSettings);
}

// If the stream exists, an attempt will be made to emit an event
// on the stream object itself. Otherwise, forward it on to the
// session (which may, in turn, forward it on to the server)
function onPriority(id, parent, weight, exclusive) {
  const session = this[kOwner];
  if (session.destroyed)
    return;
  debugStream(id, session[kType],
              'priority [parent: %d, weight: %d, exclusive: %s]',
              parent, weight, exclusive);
  const emitter = session[kState].streams.get(id) || session;
  if (!emitter.destroyed) {
    emitter[kUpdateTimer]();
    emitter.emit('priority', id, parent, weight, exclusive);
  }
}

// Called by the native layer when an error has occurred sending a
// frame. This should be exceedingly rare.
function onFrameError(id, type, code) {
  const session = this[kOwner];
  if (session.destroyed)
    return;
  debugSessionObj(session, 'error sending frame type %d on stream %d, code: %d',
                  type, id, code);
  const emitter = session[kState].streams.get(id) || session;
  emitter[kUpdateTimer]();
  emitter.emit('frameError', type, code, id);
}

function onAltSvc(stream, origin, alt) {
  const session = this[kOwner];
  if (session.destroyed)
    return;
  debugSessionObj(session, 'altsvc received: stream: %d, origin: %s, alt: %s',
                  stream, origin, alt);
  session[kUpdateTimer]();
  session.emit('altsvc', alt, origin, stream);
}

function onOrigin(origins) {
  const session = this[kOwner];
  if (session.destroyed)
    return;
  debugSessionObj(session, 'origin received: %j', origins);
  session[kUpdateTimer]();
  if (!session.encrypted || session.destroyed)
    return undefined;
  const originSet = initOriginSet(session);
  for (let n = 0; n < origins.length; n++)
    originSet.add(origins[n]);
  session.emit('origin', origins);
}

// Receiving a GOAWAY frame from the connected peer is a signal that no
// new streams should be created. If the code === NGHTTP2_NO_ERROR, we
// are going to send our close, but allow existing frames to close
// normally. If code !== NGHTTP2_NO_ERROR, we are going to send our own
// close using the same code then destroy the session with an error.
// The goaway event will be emitted on next tick.
function onGoawayData(code, lastStreamID, buf) {
  const session = this[kOwner];
  if (session.destroyed)
    return;
  debugSessionObj(session, 'goaway %d received [last stream id: %d]',
                  code, lastStreamID);

  const state = session[kState];
  state.goawayCode = code;
  state.goawayLastStreamID = lastStreamID;

  session.emit('goaway', code, lastStreamID, buf);
  if (code === NGHTTP2_NO_ERROR) {
    // If this is a no error goaway, begin shutting down.
    // No new streams permitted, but existing streams may
    // close naturally on their own.
    session.close();
  } else {
    // However, if the code is not NGHTTP_NO_ERROR, destroy the
    // session immediately. We destroy with an error but send a
    // goaway using NGHTTP2_NO_ERROR because there was no error
    // condition on this side of the session that caused the
    // shutdown.
    session.destroy(new ERR_HTTP2_SESSION_ERROR(code), NGHTTP2_NO_ERROR);
  }
}

module.exports = {
  constants: {
    kInit,
    kNativeFields,
    kOrigin,
    kOwner,
    kRemoteSettings,
    kServer,
  },
  debugSessionObj,
  onAltSvc,
  onFrameError,
  onGoawayData,
  onOrigin,
  onPing,
  onPriority,
  onSessionHeaders,
  onSessionInternalError,
  onSettings,
  sessionOnError,
  sessionOnPriority,
  sessionOnStream,
  sessionOnTimeout,
  ClientHttp2Session,
  Http2Session,
  ServerHttp2Session,
};
