'use strict';

/* eslint-disable no-use-before-define */

require('internal/util').assertCrypto();

const { async_id_symbol } = process.binding('async_wrap');
const binding = process.binding('http2');
const assert = require('assert');
const { Buffer } = require('buffer');
const EventEmitter = require('events');
const net = require('net');
const tls = require('tls');
const util = require('util');
const fs = require('fs');
const errors = require('internal/errors');
const { StreamWrap } = require('_stream_wrap');
const { Duplex } = require('stream');
const { URL } = require('url');
const { onServerStream,
        Http2ServerRequest,
        Http2ServerResponse,
} = require('internal/http2/compat');
const { utcDate } = require('internal/http');
const { promisify } = require('internal/util');
const { isArrayBufferView } = require('internal/util/types');
const { _connectionListener: httpConnectionListener } = require('http');
const { createPromise, promiseResolve } = process.binding('util');
const debug = util.debuglog('http2');

const kMaxFrameSize = (2 ** 24) - 1;
const kMaxInt = (2 ** 32) - 1;
const kMaxStreams = (2 ** 31) - 1;

// eslint-disable-next-line no-control-regex
const kQuotedString = /^[\x09\x20-\x5b\x5d-\x7e\x80-\xff]*$/;

const {
  assertIsObject,
  assertValidPseudoHeaderResponse,
  assertValidPseudoHeaderTrailer,
  assertWithinRange,
  getDefaultSettings,
  getSessionState,
  getSettings,
  getStreamState,
  isPayloadMeaningless,
  kSocket,
  mapToHeaders,
  NghttpError,
  sessionName,
  toHeaderObject,
  updateOptionsBuffer,
  updateSettingsBuffer
} = require('internal/http2/util');

const {
  kTimeout,
  setUnrefTimeout,
  validateTimerDuration,
  refreshFnSymbol
} = require('internal/timers');

const { ShutdownWrap, WriteWrap } = process.binding('stream_wrap');
const { constants } = binding;

const NETServer = net.Server;
const TLSServer = tls.Server;

const kInspect = require('internal/util').customInspectSymbol;

const kAlpnProtocol = Symbol('alpnProtocol');
const kAuthority = Symbol('authority');
const kEncrypted = Symbol('encrypted');
const kHandle = Symbol('handle');
const kID = Symbol('id');
const kInit = Symbol('init');
const kInfoHeaders = Symbol('sent-info-headers');
const kMaybeDestroy = Symbol('maybe-destroy');
const kLocalSettings = Symbol('local-settings');
const kOptions = Symbol('options');
const kOwner = Symbol('owner');
const kProceed = Symbol('proceed');
const kProtocol = Symbol('protocol');
const kProxySocket = Symbol('proxy-socket');
const kRemoteSettings = Symbol('remote-settings');
const kSentHeaders = Symbol('sent-headers');
const kSentTrailers = Symbol('sent-trailers');
const kServer = Symbol('server');
const kSession = Symbol('session');
const kState = Symbol('state');
const kType = Symbol('type');
const kUpdateTimer = Symbol('update-timer');

const kDefaultSocketTimeout = 2 * 60 * 1000;

const {
  paddingBuffer,
  PADDING_BUF_FRAME_LENGTH,
  PADDING_BUF_MAX_PAYLOAD_LENGTH,
  PADDING_BUF_RETURN_VALUE
} = binding;

const {
  NGHTTP2_CANCEL,
  NGHTTP2_REFUSED_STREAM,
  NGHTTP2_DEFAULT_WEIGHT,
  NGHTTP2_FLAG_END_STREAM,
  NGHTTP2_HCAT_PUSH_RESPONSE,
  NGHTTP2_HCAT_RESPONSE,
  NGHTTP2_INTERNAL_ERROR,
  NGHTTP2_NO_ERROR,
  NGHTTP2_SESSION_CLIENT,
  NGHTTP2_SESSION_SERVER,
  NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE,
  NGHTTP2_ERR_INVALID_ARGUMENT,
  NGHTTP2_ERR_STREAM_CLOSED,

  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_DATE,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_CONTENT_LENGTH,

  NGHTTP2_SETTINGS_HEADER_TABLE_SIZE,
  NGHTTP2_SETTINGS_ENABLE_PUSH,
  NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS,
  NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE,
  NGHTTP2_SETTINGS_MAX_FRAME_SIZE,
  NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE,

  HTTP2_METHOD_GET,
  HTTP2_METHOD_HEAD,
  HTTP2_METHOD_CONNECT,

  HTTP_STATUS_CONTINUE,
  HTTP_STATUS_RESET_CONTENT,
  HTTP_STATUS_OK,
  HTTP_STATUS_NO_CONTENT,
  HTTP_STATUS_NOT_MODIFIED,
  HTTP_STATUS_SWITCHING_PROTOCOLS,

  STREAM_OPTION_EMPTY_PAYLOAD,
  STREAM_OPTION_GET_TRAILERS
} = constants;

const STREAM_FLAGS_PENDING = 0x0;
const STREAM_FLAGS_READY = 0x1;
const STREAM_FLAGS_CLOSED = 0x2;
const STREAM_FLAGS_HEADERS_SENT = 0x4;
const STREAM_FLAGS_HEAD_REQUEST = 0x8;
const STREAM_FLAGS_ABORTED = 0x10;

const SESSION_FLAGS_PENDING = 0x0;
const SESSION_FLAGS_READY = 0x1;
const SESSION_FLAGS_CLOSED = 0x2;
const SESSION_FLAGS_DESTROYED = 0x4;

// Top level to avoid creating a closure
function emit(self, ...args) {
  self.emit(...args);
}

// Called when a new block of headers has been received for a given
// stream. The stream may or may not be new. If the stream is new,
// create the associated Http2Stream instance and emit the 'stream'
// event. If the stream is not new, emit the 'headers' event to pass
// the block of headers on.
function onSessionHeaders(handle, id, cat, flags, headers) {
  const session = this[kOwner];
  if (session.destroyed)
    return;

  const type = session[kType];
  session[kUpdateTimer]();
  debug(`Http2Stream ${id} [Http2Session ` +
        `${sessionName(type)}]: headers received`);
  const streams = session[kState].streams;

  const endOfStream = !!(flags & NGHTTP2_FLAG_END_STREAM);
  let stream = streams.get(id);

  // Convert the array of header name value pairs into an object
  const obj = toHeaderObject(headers);

  if (stream === undefined) {
    if (session.closed) {
      // we are not accepting any new streams at this point. This callback
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
    }
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
    debug(`Http2Stream ${id} [Http2Session ` +
          `${sessionName(type)}]: emitting stream '${event}' event`);
    process.nextTick(emit, stream, event, obj, flags, headers);
  }
  if (endOfStream) {
    stream.push(null);
  }
}

function tryClose(fd) {
  // Try to close the file descriptor. If closing fails, assert because
  // an error really should not happen at this point.
  fs.close(fd, (err) => assert.ifError(err));
}

// Called to determine if there are trailers to be sent at the end of a
// Stream. The 'getTrailers' callback is invoked and passed a holder object.
// The trailers to return are set on that object by the handler. Once the
// event handler returns, those are sent off for processing. Note that this
// is a necessarily synchronous operation. We need to know immediately if
// there are trailing headers to send.
function onStreamTrailers() {
  const stream = this[kOwner];
  if (stream.destroyed)
    return [];
  const trailers = Object.create(null);
  stream[kState].getTrailers.call(stream, trailers);
  const headersList = mapToHeaders(trailers, assertValidPseudoHeaderTrailer);
  if (!Array.isArray(headersList)) {
    stream.destroy(headersList);
    return [];
  }
  stream[kSentTrailers] = trailers;
  return headersList;
}

// Submit an RST-STREAM frame to be sent to the remote peer.
// This will cause the Http2Stream to be closed.
function submitRstStream(code) {
  if (this[kHandle] !== undefined) {
    this[kHandle].rstStream(code);
  }
}

// Called when the stream is closed either by sending or receiving an
// RST_STREAM frame, or through a natural end-of-stream.
// If the writable and readable sides of the stream are still open at this
// point, close them. If there is an open fd for file send, close that also.
// At this point the underlying node::http2:Http2Stream handle is no
// longer usable so destroy it also.
function onStreamClose(code) {
  const stream = this[kOwner];
  if (stream.destroyed)
    return;

  const state = stream[kState];

  debug(`Http2Stream ${stream[kID]} [Http2Session ` +
        `${sessionName(stream[kSession][kType])}]: closed with code ${code}`);

  if (!stream.closed) {
    // Clear timeout and remove timeout listeners
    stream.setTimeout(0);
    stream.removeAllListeners('timeout');

    // Set the state flags
    state.flags |= STREAM_FLAGS_CLOSED;
    state.rstCode = code;

    // Close the writable side of the stream
    abort(stream);
    stream.end();
  }

  if (state.fd !== undefined)
    tryClose(state.fd);
  stream.push(null);
  stream[kMaybeDestroy](null, code);
}

// Receives a chunk of data for a given stream and forwards it on
// to the Http2Stream Duplex for processing.
function onStreamRead(nread, buf) {
  const stream = this[kOwner];
  if (nread >= 0 && !stream.destroyed) {
    debug(`Http2Stream ${stream[kID]} [Http2Session ` +
          `${sessionName(stream[kSession][kType])}]: receiving data chunk ` +
          `of size ${nread}`);
    stream[kUpdateTimer]();
    if (!stream.push(buf)) {
      if (!stream.destroyed)  // we have to check a second time
        this.readStop();
    }
    return;
  }
  // Last chunk was received. End the readable side.
  debug(`Http2Stream ${stream[kID]} [Http2Session ` +
        `${sessionName(stream[kSession][kType])}]: ending readable.`);
  stream.push(null);
  stream[kMaybeDestroy]();
}

// Called when the remote peer settings have been updated.
// Resets the cached settings.
function onSettings() {
  const session = this[kOwner];
  if (session.destroyed)
    return;
  session[kUpdateTimer]();
  debug(`Http2Session ${sessionName(session[kType])}: new settings received`);
  session[kRemoteSettings] = undefined;
  process.nextTick(emit, session, 'remoteSettings', session.remoteSettings);
}

// If the stream exists, an attempt will be made to emit an event
// on the stream object itself. Otherwise, forward it on to the
// session (which may, in turn, forward it on to the server)
function onPriority(id, parent, weight, exclusive) {
  const session = this[kOwner];
  if (session.destroyed)
    return;
  debug(`Http2Stream ${id} [Http2Session ` +
        `${sessionName(session[kType])}]: priority [parent: ${parent}, ` +
        `weight: ${weight}, exclusive: ${exclusive}]`);
  const emitter = session[kState].streams.get(id) || session;
  if (!emitter.destroyed) {
    emitter[kUpdateTimer]();
    process.nextTick(emit, emitter, 'priority', id, parent, weight, exclusive);
  }
}

// Called by the native layer when an error has occurred sending a
// frame. This should be exceedingly rare.
function onFrameError(id, type, code) {
  const session = this[kOwner];
  if (session.destroyed)
    return;
  debug(`Http2Session ${sessionName(session[kType])}: error sending frame ` +
        `type ${type} on stream ${id}, code: ${code}`);
  const emitter = session[kState].streams.get(id) || session;
  emitter[kUpdateTimer]();
  process.nextTick(emit, emitter, 'frameError', type, code, id);
}

function onAltSvc(stream, origin, alt) {
  const session = this[kOwner];
  if (session.destroyed)
    return;
  debug(`Http2Session ${sessionName(session[kType])}: altsvc received: ` +
        `stream: ${stream}, origin: ${origin}, alt: ${alt}`);
  session[kUpdateTimer]();
  process.nextTick(emit, session, 'altsvc', alt, origin, stream);
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
  debug(`Http2Session ${sessionName(session[kType])}: goaway ${code} ` +
        `received [last stream id: ${lastStreamID}]`);

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
    session.destroy(new errors.Error('ERR_HTTP2_SESSION_ERROR', code),
                    { errorCode: NGHTTP2_NO_ERROR });
  }
}

// Returns the padding to use per frame. The selectPadding callback is set
// on the options. It is invoked with two arguments, the frameLen, and the
// maxPayloadLen. The method must return a numeric value within the range
// frameLen <= n <= maxPayloadLen.
function onSelectPadding(fn) {
  return function getPadding() {
    const frameLen = paddingBuffer[PADDING_BUF_FRAME_LENGTH];
    const maxFramePayloadLen = paddingBuffer[PADDING_BUF_MAX_PAYLOAD_LENGTH];
    paddingBuffer[PADDING_BUF_RETURN_VALUE] = fn(frameLen, maxFramePayloadLen);
  };
}

// When a ClientHttp2Session is first created, the socket may not yet be
// connected. If request() is called during this time, the actual request
// will be deferred until the socket is ready to go.
function requestOnConnect(headers, options) {
  const session = this[kSession];

  // At this point, the stream should have already been destroyed during
  // the session.destroy() method. Do nothing else.
  if (session.destroyed)
    return;

  // If the session was closed while waiting for the connect, destroy
  // the stream and do not continue with the request.
  if (session.closed) {
    const err = new errors.Error('ERR_HTTP2_GOAWAY_SESSION');
    this.destroy(err);
    return;
  }

  debug(`Http2Session ${sessionName(session[kType])}: connected, ` +
        'initializing request');

  let streamOptions = 0;
  if (options.endStream)
    streamOptions |= STREAM_OPTION_EMPTY_PAYLOAD;

  if (typeof options.getTrailers === 'function') {
    streamOptions |= STREAM_OPTION_GET_TRAILERS;
    this[kState].getTrailers = options.getTrailers;
  }

  // ret will be either the reserved stream ID (if positive)
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
        err = new errors.Error('ERR_HTTP2_OUT_OF_STREAMS');
        this.destroy(err);
        break;
      case NGHTTP2_ERR_INVALID_ARGUMENT:
        err = new errors.Error('ERR_HTTP2_STREAM_SELF_DEPENDENCY');
        this.destroy(err);
        break;
      default:
        session.destroy(new NghttpError(ret));
    }
    return;
  }
  this[kInit](ret.id(), ret);
}

// Validates that priority options are correct, specifically:
// 1. options.weight must be a number
// 2. options.parent must be a positive number
// 3. options.exclusive must be a boolean
// 4. if specified, options.silent must be a boolean
//
// Also sets the default priority options if they are not set.
function validatePriorityOptions(options) {
  let err;
  if (options.weight === undefined) {
    options.weight = NGHTTP2_DEFAULT_WEIGHT;
  } else if (typeof options.weight !== 'number') {
    err = new errors.TypeError('ERR_INVALID_OPT_VALUE',
                               'weight',
                               options.weight);
  }

  if (options.parent === undefined) {
    options.parent = 0;
  } else if (typeof options.parent !== 'number' || options.parent < 0) {
    err = new errors.TypeError('ERR_INVALID_OPT_VALUE',
                               'parent',
                               options.parent);
  }

  if (options.exclusive === undefined) {
    options.exclusive = false;
  } else if (typeof options.exclusive !== 'boolean') {
    err = new errors.TypeError('ERR_INVALID_OPT_VALUE',
                               'exclusive',
                               options.exclusive);
  }

  if (options.silent === undefined) {
    options.silent = false;
  } else if (typeof options.silent !== 'boolean') {
    err = new errors.TypeError('ERR_INVALID_OPT_VALUE',
                               'silent',
                               options.silent);
  }

  if (err) {
    Error.captureStackTrace(err, validatePriorityOptions);
    throw err;
  }
}

// When an error occurs internally at the binding level, immediately
// destroy the session.
function onSessionInternalError(code) {
  if (this[kOwner] !== undefined)
    this[kOwner].destroy(new NghttpError(code));
}

function settingsCallback(cb, ack, duration) {
  this[kState].pendingAck--;
  this[kLocalSettings] = undefined;
  if (ack) {
    debug(`Http2Session ${sessionName(this[kType])}: settings received`);
    const settings = this.localSettings;
    if (typeof cb === 'function')
      cb(null, settings, duration);
    this.emit('localSettings', settings);
  } else {
    debug(`Http2Session ${sessionName(this[kType])}: settings canceled`);
    if (typeof cb === 'function')
      cb(new errors.Error('ERR_HTTP2_SETTINGS_CANCEL'));
  }
}

// Submits a SETTINGS frame to be sent to the remote peer.
function submitSettings(settings, callback) {
  if (this.destroyed)
    return;
  debug(`Http2Session ${sessionName(this[kType])}: submitting settings`);
  this[kUpdateTimer]();
  updateSettingsBuffer(settings);
  if (!this[kHandle].settings(settingsCallback.bind(this, callback))) {
    this.destroy(new errors.Error('ERR_HTTP2_MAX_PENDING_SETTINGS_ACK'));
  }
}

// Submits a PRIORITY frame to be sent to the remote peer
// Note: If the silent option is true, the change will be made
// locally with no PRIORITY frame sent.
function submitPriority(options) {
  if (this.destroyed)
    return;
  this[kUpdateTimer]();

  // If the parent is the id, do nothing because a
  // stream cannot be made to depend on itself.
  if (options.parent === this[kID])
    return;

  this[kHandle].priority(options.parent | 0,
                         options.weight | 0,
                         !!options.exclusive,
                         !!options.silent);
}

// Submit a GOAWAY frame to be sent to the remote peer.
// If the lastStreamID is set to <= 0, then the lastProcStreamID will
// be used. The opaqueData must either be a typed array or undefined
// (which will be checked elsewhere).
function submitGoaway(code, lastStreamID, opaqueData) {
  if (this.destroyed)
    return;
  debug(`Http2Session ${sessionName(this[kType])}: submitting goaway`);
  this[kUpdateTimer]();
  this[kHandle].goaway(code, lastStreamID, opaqueData);
}

const proxySocketHandler = {
  get(session, prop) {
    switch (prop) {
      case 'setTimeout':
        return session.setTimeout.bind(session);
      case 'destroy':
      case 'emit':
      case 'end':
      case 'pause':
      case 'read':
      case 'resume':
      case 'write':
        throw new errors.Error('ERR_HTTP2_NO_SOCKET_MANIPULATION');
      default:
        const socket = session[kSocket];
        const value = socket[prop];
        return typeof value === 'function' ? value.bind(socket) : value;
    }
  },
  getPrototypeOf(session) {
    return Reflect.getPrototypeOf(session[kSocket]);
  },
  set(session, prop, value) {
    switch (prop) {
      case 'setTimeout':
        session.setTimeout = value;
        return true;
      case 'destroy':
      case 'emit':
      case 'end':
      case 'pause':
      case 'read':
      case 'resume':
      case 'write':
        throw new errors.Error('ERR_HTTP2_NO_SOCKET_MANIPULATION');
      default:
        session[kSocket][prop] = value;
        return true;
    }
  }
};

// pingCallback() returns a function that is invoked when an HTTP2 PING
// frame acknowledgement is received. The ack is either true or false to
// indicate if the ping was successful or not. The duration indicates the
// number of milliseconds elapsed since the ping was sent and the ack
// received. The payload is a Buffer containing the 8 bytes of payload
// data received on the PING acknowlegement.
function pingCallback(cb) {
  return function pingCallback(ack, duration, payload) {
    const err = ack ? null : new errors.Error('ERR_HTTP2_PING_CANCEL');
    cb(err, duration, payload);
  };
}

// Validates the values in a settings object. Specifically:
// 1. headerTableSize must be a number in the range 0 <= n <= kMaxInt
// 2. initialWindowSize must be a number in the range 0 <= n <= kMaxInt
// 3. maxFrameSize must be a number in the range 16384 <= n <= kMaxFrameSize
// 4. maxConcurrentStreams must be a number in the range 0 <= n <= kMaxStreams
// 5. maxHeaderListSize must be a number in the range 0 <= n <= kMaxInt
// 6. enablePush must be a boolean
// All settings are optional and may be left undefined
function validateSettings(settings) {
  settings = Object.assign({}, settings);
  assertWithinRange('headerTableSize',
                    settings.headerTableSize,
                    0, kMaxInt);
  assertWithinRange('initialWindowSize',
                    settings.initialWindowSize,
                    0, kMaxInt);
  assertWithinRange('maxFrameSize',
                    settings.maxFrameSize,
                    16384, kMaxFrameSize);
  assertWithinRange('maxConcurrentStreams',
                    settings.maxConcurrentStreams,
                    0, kMaxStreams);
  assertWithinRange('maxHeaderListSize',
                    settings.maxHeaderListSize,
                    0, kMaxInt);
  if (settings.enablePush !== undefined &&
      typeof settings.enablePush !== 'boolean') {
    const err = new errors.TypeError('ERR_HTTP2_INVALID_SETTING_VALUE',
                                     'enablePush', settings.enablePush);
    err.actual = settings.enablePush;
    Error.captureStackTrace(err, 'validateSettings');
    throw err;
  }
  return settings;
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
  debug(`Http2Session ${sessionName(type)}: setting up session handle`);
  this[kState].flags |= SESSION_FLAGS_READY;

  updateOptionsBuffer(options);
  const handle = new binding.Http2Session(type);
  handle[kOwner] = this;
  handle.error = onSessionInternalError;
  handle.onpriority = onPriority;
  handle.onsettings = onSettings;
  handle.onheaders = onSessionHeaders;
  handle.onframeerror = onFrameError;
  handle.ongoawaydata = onGoawayData;
  handle.onaltsvc = onAltSvc;

  if (typeof options.selectPadding === 'function')
    handle.ongetpadding = onSelectPadding(options.selectPadding);

  assert(socket._handle !== undefined,
         'Internal HTTP/2 Failure. The socket is not connected. Please ' +
         'report this as a bug in Node.js');
  handle.consume(socket._handle._externalStream);

  this[kHandle] = handle;

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

  const settings = typeof options.settings === 'object' ?
    options.settings : {};

  this.settings(settings);
  process.nextTick(emit, this, 'connect', this, socket);
}

// Emits a close event followed by an error event if err is truthy. Used
// by Http2Session.prototype.destroy()
function emitClose(self, error) {
  if (error)
    self.emit('error', error);
  self.emit('close');
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

    if (!socket._handle || !socket._handle._externalStream) {
      socket = new StreamWrap(socket);
    }

    // No validation is performed on the input parameters because this
    // constructor is not exported directly for users.

    // If the session property already exists on the socket,
    // then it has already been bound to an Http2Session instance
    // and cannot be attached again.
    if (socket[kSession] !== undefined)
      throw new errors.Error('ERR_HTTP2_SOCKET_BOUND');

    socket[kSession] = this;

    this[kState] = {
      flags: SESSION_FLAGS_PENDING,
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

    // Do not use nagle's algorithm
    if (typeof socket.setNoDelay === 'function')
      socket.setNoDelay();

    // Disable TLS renegotiation on the socket
    if (typeof socket.disableRenegotiation === 'function')
      socket.disableRenegotiation();

    const setupFn = setupHandle.bind(this, socket, type, options);
    if (socket.connecting) {
      const connectEvent =
        socket instanceof tls.TLSSocket ? 'secureConnect' : 'connect';
      socket.once(connectEvent, setupFn);
    } else {
      setupFn();
    }

    debug(`Http2Session ${sessionName(type)}: created`);
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

    let originSet = this[kState].originSet;
    if (originSet === undefined) {
      const socket = this[kSocket];
      this[kState].originSet = originSet = new Set();
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

    return Array.from(originSet);
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
    if (this[kTimeout]) this[kTimeout][refreshFnSymbol]();
  }

  // Sets the id of the next stream to be created by this Http2Session.
  // The value must be a number in the range 0 <= n <= kMaxStreams. The
  // value also needs to be larger than the current next stream ID.
  setNextStreamID(id) {
    if (this.destroyed)
      throw new errors.Error('ERR_HTTP2_INVALID_SESSION');

    if (typeof id !== 'number')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'id', 'number');
    if (id <= 0 || id > kMaxStreams)
      throw new errors.RangeError('ERR_OUT_OF_RANGE', 'id',
                                  `> 0 and <= ${kMaxStreams}`, id);
    this[kHandle].setNextStreamID(id);
  }

  // If ping is called while we are still connecting, or after close() has
  // been called, the ping callback will be invoked immediately will a ping
  // cancelled error and a duration of 0.0.
  ping(payload, callback) {
    if (this.destroyed)
      throw new errors.Error('ERR_HTTP2_INVALID_SESSION');

    if (typeof payload === 'function') {
      callback = payload;
      payload = undefined;
    }
    if (payload && !isArrayBufferView(payload)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'payload',
                                 ['Buffer', 'TypedArray', 'DataView']);
    }
    if (payload && payload.length !== 8) {
      throw new errors.RangeError('ERR_HTTP2_PING_LENGTH');
    }
    if (typeof callback !== 'function')
      throw new errors.TypeError('ERR_INVALID_CALLBACK');

    const cb = pingCallback(callback);
    if (this.connecting || this.closed) {
      process.nextTick(cb, false, 0.0, payload);
      return;
    }

    return this[kHandle].ping(payload, cb);
  }

  [kInspect](depth, opts) {
    const obj = {
      type: this[kType],
      closed: this.closed,
      destroyed: this.destroyed,
      state: this.state,
      localSettings: this.localSettings,
      remoteSettings: this.remoteSettings
    };
    return `Http2Session ${util.format(obj)}`;
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

  // true if the Http2Session is waiting for a settings acknowledgement
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
    const settings = this[kRemoteSettings];
    if (settings !== undefined)
      return settings;

    if (this.destroyed || this.connecting)
      return {};

    return this[kRemoteSettings] = getSettings(this[kHandle], true); // Remote
  }

  // Submits a SETTINGS frame to be sent to the remote peer.
  settings(settings, callback) {
    if (this.destroyed)
      throw new errors.Error('ERR_HTTP2_INVALID_SESSION');
    assertIsObject(settings, 'settings');
    settings = validateSettings(settings);

    if (callback && typeof callback !== 'function')
      throw new errors.TypeError('ERR_INVALID_CALLBACK');
    debug(`Http2Session ${sessionName(this[kType])}: sending settings`);

    this[kState].pendingAck++;

    const settingsFn = submitSettings.bind(this, settings, callback);
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
      throw new errors.Error('ERR_HTTP2_INVALID_SESSION');

    if (opaqueData !== undefined && !isArrayBufferView(opaqueData)) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'opaqueData',
                                 ['Buffer', 'TypedArray', 'DataView']);
    }
    if (typeof code !== 'number') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'code', 'number');
    }
    if (typeof lastStreamID !== 'number') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'lastStreamID', 'number');
    }

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
    debug(`Http2Session ${sessionName(this[kType])}: destroying`);

    if (typeof error === 'number') {
      code = error;
      error =
        code !== NGHTTP2_NO_ERROR ?
          new errors.Error('ERR_HTTP2_SESSION_ERROR', code) : undefined;
    }
    if (code === undefined && error != null)
      code = NGHTTP2_INTERNAL_ERROR;

    const state = this[kState];
    state.flags |= SESSION_FLAGS_DESTROYED;

    // Clear timeout and remove timeout listeners
    this.setTimeout(0);
    this.removeAllListeners('timeout');

    // Destroy any pending and open streams
    const cancel = new errors.Error('ERR_HTTP2_STREAM_CANCEL');
    state.pendingStreams.forEach((stream) => stream.destroy(cancel));
    state.streams.forEach((stream) => stream.destroy(error));

    // Disassociate from the socket and server
    const socket = this[kSocket];
    const handle = this[kHandle];

    // Destroy the handle if it exists at this point
    if (handle !== undefined)
      handle.destroy(code, socket.destroyed);

    // If there is no error, use setImmediate to destroy the socket on the
    // next iteration of the event loop in order to give data time to transmit.
    // Otherwise, destroy immediately.
    if (!socket.destroyed) {
      if (!error) {
        setImmediate(socket.end.bind(socket));
      } else {
        socket.destroy(error);
      }
    }

    this[kProxySocket] = undefined;
    this[kSocket] = undefined;
    this[kHandle] = undefined;
    socket[kSession] = undefined;
    socket[kServer] = undefined;

    // Finally, emit the close and error events (if necessary) on next tick.
    process.nextTick(emitClose, this, error);
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
    debug(`Http2Session ${sessionName(this[kType])}: marking session closed`);
    this[kState].flags |= SESSION_FLAGS_CLOSED;
    if (typeof callback === 'function')
      this.once('close', callback);
    this.goaway();
    this[kMaybeDestroy]();
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
    // If the session is destroyed, this should never actually be invoked,
    // but just in case...
    if (this.destroyed)
      return;
    // This checks whether a write is currently in progress and also whether
    // that write is actually sending data across the write. The kHandle
    // stored `chunksSentSinceLastWrite` is only updated when a timeout event
    // happens, meaning that if a write is ongoing it should never equal the
    // newly fetched, updated value.
    if (this[kState].writeQueueSize > 0) {
      const handle = this[kHandle];
      const chunksSentSinceLastWrite = handle !== undefined ?
        handle.chunksSentSinceLastWrite : null;
      if (chunksSentSinceLastWrite !== null &&
          chunksSentSinceLastWrite !== handle.updateChunksSent()) {
        this[kUpdateTimer]();
        return;
      }
    }

    process.nextTick(emit, this, 'timeout');
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
      throw new errors.Error('ERR_HTTP2_INVALID_SESSION');

    let stream = 0;
    let origin;

    if (typeof originOrStream === 'string') {
      origin = (new URL(originOrStream)).origin;
      if (origin === 'null')
        throw new errors.TypeError('ERR_HTTP2_ALTSVC_INVALID_ORIGIN');
    } else if (typeof originOrStream === 'number') {
      if (originOrStream >>> 0 !== originOrStream || originOrStream === 0)
        throw new errors.RangeError('ERR_OUT_OF_RANGE', 'originOrStream');
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
        throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'originOrStream',
                                   ['string', 'number', 'URL', 'object']);
      } else if (origin === 'null' || origin.length === 0) {
        throw new errors.TypeError('ERR_HTTP2_ALTSVC_INVALID_ORIGIN');
      }
    }

    if (typeof alt !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'alt', 'string');
    if (!kQuotedString.test(alt))
      throw new errors.TypeError('ERR_INVALID_CHAR', 'alt');

    // Max length permitted for ALTSVC
    if ((alt.length + (origin !== undefined ? origin.length : 0)) > 16382)
      throw new errors.TypeError('ERR_HTTP2_ALTSVC_LENGTH');

    this[kHandle].altsvc(stream, origin || '', alt);
  }
}

// ClientHttp2Session instances have to wait for the socket to connect after
// they have been created. Various operations such as request() may be used,
// but the actual protocol communication will only occur after the socket
// has been connected.
class ClientHttp2Session extends Http2Session {
  constructor(options, socket) {
    super(NGHTTP2_SESSION_CLIENT, options, socket);
  }

  // Submits a new HTTP2 request to the connected peer. Returns the
  // associated Http2Stream instance.
  request(headers, options) {
    debug(`Http2Session ${sessionName(this[kType])}: initiating request`);

    if (this.destroyed)
      throw new errors.Error('ERR_HTTP2_INVALID_SESSION');

    if (this.closed)
      throw new errors.Error('ERR_HTTP2_GOAWAY_SESSION');

    this[kUpdateTimer]();

    assertIsObject(headers, 'headers');
    assertIsObject(options, 'options');

    headers = Object.assign(Object.create(null), headers);
    options = Object.assign({}, options);

    if (headers[HTTP2_HEADER_METHOD] === undefined)
      headers[HTTP2_HEADER_METHOD] = HTTP2_METHOD_GET;

    const connect = headers[HTTP2_HEADER_METHOD] === HTTP2_METHOD_CONNECT;

    if (!connect) {
      if (headers[HTTP2_HEADER_AUTHORITY] === undefined)
        headers[HTTP2_HEADER_AUTHORITY] = this[kAuthority];
      if (headers[HTTP2_HEADER_SCHEME] === undefined)
        headers[HTTP2_HEADER_SCHEME] = this[kProtocol].slice(0, -1);
      if (headers[HTTP2_HEADER_PATH] === undefined)
        headers[HTTP2_HEADER_PATH] = '/';
    } else {
      if (headers[HTTP2_HEADER_AUTHORITY] === undefined)
        throw new errors.Error('ERR_HTTP2_CONNECT_AUTHORITY');
      if (headers[HTTP2_HEADER_SCHEME] !== undefined)
        throw new errors.Error('ERR_HTTP2_CONNECT_SCHEME');
      if (headers[HTTP2_HEADER_PATH] !== undefined)
        throw new errors.Error('ERR_HTTP2_CONNECT_PATH');
    }

    validatePriorityOptions(options);

    if (options.endStream === undefined) {
      // For some methods, we know that a payload is meaningless, so end the
      // stream by default if the user has not specifically indicated a
      // preference.
      options.endStream = isPayloadMeaningless(headers[HTTP2_HEADER_METHOD]);
    } else if (typeof options.endStream !== 'boolean') {
      throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                 'endStream',
                                 options.endStream);
    }

    if (options.getTrailers !== undefined &&
      typeof options.getTrailers !== 'function') {
      throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                 'getTrailers',
                                 options.getTrailers);
    }

    const headersList = mapToHeaders(headers);
    if (!Array.isArray(headersList))
      throw headersList;

    const stream = new ClientHttp2Stream(this, undefined, undefined, {});
    stream[kSentHeaders] = headers;

    // Close the writable side of the stream if options.endStream is set.
    if (options.endStream)
      stream.end();

    const onConnect = requestOnConnect.bind(stream, headersList, options);
    if (this.connecting) {
      this.on('connect', onConnect);
    } else {
      onConnect();
    }
    return stream;
  }
}

function createWriteReq(req, handle, data, encoding) {
  switch (encoding) {
    case 'utf8':
    case 'utf-8':
      return handle.writeUtf8String(req, data);
    case 'ascii':
      return handle.writeAsciiString(req, data);
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      return handle.writeUcs2String(req, data);
    case 'latin1':
    case 'binary':
      return handle.writeLatin1String(req, data);
    case 'buffer':
      return handle.writeBuffer(req, data);
    default:
      return handle.writeBuffer(req, Buffer.from(data, encoding));
  }
}

function trackWriteState(stream, bytes) {
  const session = stream[kSession];
  stream[kState].writeQueueSize += bytes;
  session[kState].writeQueueSize += bytes;
  session[kHandle].chunksSentSinceLastWrite = 0;
}

function afterDoStreamWrite(status, handle, req) {
  const stream = handle[kOwner];
  const session = stream[kSession];

  stream[kUpdateTimer]();

  const { bytes } = req;
  stream[kState].writeQueueSize -= bytes;

  if (session !== undefined)
    session[kState].writeQueueSize -= bytes;
  if (typeof req.callback === 'function')
    req.callback(null);
  req.handle = undefined;
}

function onHandleFinish() {
  const handle = this[kHandle];
  if (this[kID] === undefined) {
    this.once('ready', onHandleFinish);
  } else if (handle !== undefined) {
    const req = new ShutdownWrap();
    req.oncomplete = () => {};
    req.handle = handle;
    handle.shutdown(req);
  }
}

function streamOnResume() {
  if (!this.destroyed && !this.pending)
    this[kHandle].readStart();
}

function streamOnPause() {
  if (!this.destroyed && !this.pending)
    this[kHandle].readStop();
}

// If the writable side of the Http2Stream is still open, emit the
// 'aborted' event and set the aborted flag.
function abort(stream) {
  if (!stream.aborted &&
      !(stream._writableState.ended || stream._writableState.ending)) {
    process.nextTick(emit, stream, 'aborted');
    stream[kState].flags |= STREAM_FLAGS_ABORTED;
  }
}

// An Http2Stream is a Duplex stream that is backed by a
// node::http2::Http2Stream handle implementing StreamBase.
class Http2Stream extends Duplex {
  constructor(session, options) {
    options.allowHalfOpen = true;
    options.decodeStrings = false;
    super(options);
    this[async_id_symbol] = -1;

    // Corking the stream automatically allows writes to happen
    // but ensures that those are buffered until the handle has
    // been assigned.
    this.cork();
    this[kSession] = session;
    session[kState].pendingStreams.add(this);

    this[kTimeout] = null;

    this[kState] = {
      flags: STREAM_FLAGS_PENDING,
      rstCode: NGHTTP2_NO_ERROR,
      writeQueueSize: 0
    };

    this.once('finish', onHandleFinish);
    this.on('resume', streamOnResume);
    this.on('pause', streamOnPause);
  }

  [kUpdateTimer]() {
    if (this.destroyed)
      return;
    if (this[kTimeout])
      this[kTimeout][refreshFnSymbol]();
    if (this[kSession])
      this[kSession][kUpdateTimer]();
  }

  [kInit](id, handle) {
    const state = this[kState];
    state.flags |= STREAM_FLAGS_READY;

    const session = this[kSession];
    session[kState].pendingStreams.delete(this);
    session[kState].streams.set(id, this);

    this[kID] = id;
    this[async_id_symbol] = handle.getAsyncId();
    handle[kOwner] = this;
    this[kHandle] = handle;
    handle.ontrailers = onStreamTrailers;
    handle.onstreamclose = onStreamClose;
    handle.onread = onStreamRead;
    this.uncork();
    this.emit('ready');
  }

  [kInspect](depth, opts) {
    const obj = {
      id: this[kID] || '<pending>',
      closed: this.closed,
      destroyed: this.destroyed,
      state: this.state,
      readableState: this._readableState,
      writableState: this._writableState
    };
    return `Http2Stream ${util.format(obj)}`;
  }

  get sentHeaders() {
    return this[kSentHeaders];
  }

  get sentTrailers() {
    return this[kSentTrailers];
  }

  get sentInfoHeaders() {
    return this[kInfoHeaders];
  }

  get pending() {
    return this[kID] === undefined;
  }

  // The id of the Http2Stream, will be undefined if the socket is not
  // yet connected.
  get id() {
    return this[kID];
  }

  // The Http2Session that owns this Http2Stream.
  get session() {
    return this[kSession];
  }

  _onTimeout() {
    if (this.destroyed)
      return;
    // This checks whether a write is currently in progress and also whether
    // that write is actually sending data across the write. The kHandle
    // stored `chunksSentSinceLastWrite` is only updated when a timeout event
    // happens, meaning that if a write is ongoing it should never equal the
    // newly fetched, updated value.
    if (this[kState].writeQueueSize > 0) {
      const handle = this[kSession][kHandle];
      const chunksSentSinceLastWrite = handle !== undefined ?
        handle.chunksSentSinceLastWrite : null;
      if (chunksSentSinceLastWrite !== null &&
          chunksSentSinceLastWrite !== handle.updateChunksSent()) {
        this[kUpdateTimer]();
        return;
      }
    }

    process.nextTick(emit, this, 'timeout');
  }

  // true if the HEADERS frame has been sent
  get headersSent() {
    return !!(this[kState].flags & STREAM_FLAGS_HEADERS_SENT);
  }

  // true if the Http2Stream was aborted abnormally.
  get aborted() {
    return !!(this[kState].flags & STREAM_FLAGS_ABORTED);
  }

  // true if dealing with a HEAD request
  get headRequest() {
    return !!(this[kState].flags & STREAM_FLAGS_HEAD_REQUEST);
  }

  // The error code reported when this Http2Stream was closed.
  get rstCode() {
    return this[kState].rstCode;
  }

  // State information for the Http2Stream
  get state() {
    const id = this[kID];
    if (this.destroyed || id === undefined)
      return {};
    return getStreamState(this[kHandle], id);
  }

  [kProceed]() {
    assert.fail(null, null,
                'Implementors MUST implement this. Please report this as a ' +
                'bug in Node.js');
  }

  _write(data, encoding, cb) {
    // When the Http2Stream is first created, it is corked until the
    // handle and the stream ID is assigned. However, if the user calls
    // uncork() before that happens, the Duplex will attempt to pass
    // writes through. Those need to be queued up here.
    if (this.pending) {
      this.once('ready', this._write.bind(this, data, encoding, cb));
      return;
    }

    // If the stream has been destroyed, there's nothing else we can do
    // because the handle has been destroyed. This should only be an
    // issue if a write occurs before the 'ready' event in the case where
    // the duplex is uncorked before the stream is ready to go. In that
    // case, drop the data on the floor. An error should have already been
    // emitted.
    if (this.destroyed)
      return;

    this[kUpdateTimer]();
    if (!this.headersSent)
      this[kProceed]();

    const handle = this[kHandle];
    const req = new WriteWrap();
    req.stream = this[kID];
    req.handle = handle;
    req.callback = cb;
    req.oncomplete = afterDoStreamWrite;
    req.async = false;
    const err = createWriteReq(req, handle, data, encoding);
    if (err)
      throw util._errnoException(err, 'write', req.error);
    trackWriteState(this, req.bytes);
  }

  _writev(data, cb) {
    // When the Http2Stream is first created, it is corked until the
    // handle and the stream ID is assigned. However, if the user calls
    // uncork() before that happens, the Duplex will attempt to pass
    // writes through. Those need to be queued up here.
    if (this.pending) {
      this.once('ready', this._writev.bind(this, data, cb));
      return;
    }

    // If the stream has been destroyed, there's nothing else we can do
    // because the handle has been destroyed. This should only be an
    // issue if a write occurs before the 'ready' event in the case where
    // the duplex is uncorked before the stream is ready to go. In that
    // case, drop the data on the floor. An error should have already been
    // emitted.
    if (this.destroyed)
      return;

    this[kUpdateTimer]();

    if (!this.headersSent)
      this[kProceed]();

    const handle = this[kHandle];
    const req = new WriteWrap();
    req.stream = this[kID];
    req.handle = handle;
    req.callback = cb;
    req.oncomplete = afterDoStreamWrite;
    req.async = false;
    const chunks = new Array(data.length << 1);
    for (var i = 0; i < data.length; i++) {
      const entry = data[i];
      chunks[i * 2] = entry.chunk;
      chunks[i * 2 + 1] = entry.encoding;
    }
    const err = handle.writev(req, chunks);
    if (err)
      throw util._errnoException(err, 'write', req.error);
    trackWriteState(this, req.bytes);
  }

  _read(nread) {
    if (this.destroyed) {
      this.push(null);
      return;
    }
    if (!this.pending) {
      streamOnResume.call(this);
    } else {
      this.once('ready', streamOnResume);
    }
  }

  priority(options) {
    if (this.destroyed)
      throw new errors.Error('ERR_HTTP2_INVALID_STREAM');

    assertIsObject(options, 'options');
    options = Object.assign({}, options);
    validatePriorityOptions(options);

    const priorityFn = submitPriority.bind(this, options);

    // If the handle has not yet been assigned, queue up the priority
    // frame to be sent as soon as the ready event is emitted.
    if (this.pending) {
      this.once('ready', priorityFn);
      return;
    }
    priorityFn();
  }

  get closed() {
    return !!(this[kState].flags & STREAM_FLAGS_CLOSED);
  }

  // Close initiates closing the Http2Stream instance by sending an RST_STREAM
  // frame to the connected peer. The readable and writable sides of the
  // Http2Stream duplex are closed and the timeout timer is cleared. If
  // a callback is passed, it is registered to listen for the 'close' event.
  //
  // If the handle and stream ID have not been assigned yet, the close
  // will be queued up to wait for the ready event. As soon as the stream ID
  // is determined, the close will proceed.
  //
  // Submitting the RST_STREAM frame to the underlying handle will cause
  // the Http2Stream to be closed and ultimately destroyed. After calling
  // close, it is still possible to queue up PRIORITY and RST_STREAM frames,
  // but no DATA and HEADERS frames may be sent.
  close(code = NGHTTP2_NO_ERROR, callback) {
    if (typeof code !== 'number')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'code', 'number');
    if (code < 0 || code > kMaxInt)
      throw new errors.RangeError('ERR_OUT_OF_RANGE', 'code');

    // Clear timeout and remove timeout listeners
    this.setTimeout(0);
    this.removeAllListeners('timeout');

    // Close the writable
    abort(this);
    this.end();

    if (this.closed)
      return;

    const state = this[kState];
    state.flags |= STREAM_FLAGS_CLOSED;
    state.rstCode = code;

    if (callback !== undefined) {
      if (typeof callback !== 'function')
        throw new errors.TypeError('ERR_INVALID_CALLBACK');
      this.once('close', callback);
    }

    if (this[kHandle] === undefined)
      return;

    const rstStreamFn = submitRstStream.bind(this, code);
    // If the handle has not yet been assigned, queue up the request to
    // ensure that the RST_STREAM frame is sent after the stream ID has
    // been determined.
    if (this.pending) {
      this.push(null);
      this.once('ready', rstStreamFn);
      return;
    }
    rstStreamFn();
  }

  // Called by this.destroy().
  // * Will submit an RST stream to shutdown the stream if necessary.
  //   This will cause the internal resources to be released.
  // * Then cleans up the resources on the js side
  _destroy(err, callback) {
    const session = this[kSession];
    const handle = this[kHandle];
    const id = this[kID];

    debug(`Http2Stream ${this[kID] || '<pending>'} [Http2Session ` +
          `${sessionName(session[kType])}]: destroying stream`);
    const state = this[kState];
    const code = state.rstCode =
      err != null ?
        NGHTTP2_INTERNAL_ERROR :
        state.rstCode || NGHTTP2_NO_ERROR;
    if (handle !== undefined) {
      // If the handle exists, we need to close, then destroy the handle
      this.close(code);
      if (!this._readableState.ended && !this._readableState.ending)
        this.push(null);
      handle.destroy();
      session[kState].streams.delete(id);
    } else {
      // Clear timeout and remove timeout listeners
      this.setTimeout(0);
      this.removeAllListeners('timeout');

      state.flags |= STREAM_FLAGS_CLOSED;
      abort(this);
      this.end();
      this.push(null);
      session[kState].pendingStreams.delete(this);
    }

    // Adjust the write queue size for accounting
    session[kState].writeQueueSize -= state.writeQueueSize;
    state.writeQueueSize = 0;

    // RST code 8 not emitted as an error as its used by clients to signify
    // abort and is already covered by aborted event, also allows more
    // seamless compatibility with http1
    if (err == null && code !== NGHTTP2_NO_ERROR && code !== NGHTTP2_CANCEL)
      err = new errors.Error('ERR_HTTP2_STREAM_ERROR', code);

    this[kSession] = undefined;
    this[kHandle] = undefined;

    // This notifies the session that this stream has been destroyed and
    // gives the session the opportunity to clean itself up. The session
    // will destroy if it has been closed and there are no other open or
    // pending streams.
    session[kMaybeDestroy]();
    process.nextTick(emit, this, 'close', code);
    callback(err);
  }

  // The Http2Stream can be destroyed if it has closed and if the readable
  // side has received the final chunk.
  [kMaybeDestroy](error, code = NGHTTP2_NO_ERROR) {
    if (error == null) {
      if (code === NGHTTP2_NO_ERROR &&
          (!this._readableState.ended ||
          !this._writableState.ended ||
          this._writableState.pendingcb > 0 ||
          !this.closed)) {
        return;
      }
    }
    this.destroy(error);
  }
}

function processHeaders(headers) {
  assertIsObject(headers, 'headers');
  headers = Object.assign(Object.create(null), headers);
  if (headers[HTTP2_HEADER_STATUS] == null)
    headers[HTTP2_HEADER_STATUS] = HTTP_STATUS_OK;
  headers[HTTP2_HEADER_DATE] = utcDate();

  const statusCode = headers[HTTP2_HEADER_STATUS] |= 0;
  // This is intentionally stricter than the HTTP/1 implementation, which
  // allows values between 100 and 999 (inclusive) in order to allow for
  // backwards compatibility with non-spec compliant code. With HTTP/2,
  // we have the opportunity to start fresh with stricter spec compliance.
  // This will have an impact on the compatibility layer for anyone using
  // non-standard, non-compliant status codes.
  if (statusCode < 200 || statusCode > 599)
    throw new errors.RangeError('ERR_HTTP2_STATUS_INVALID',
                                headers[HTTP2_HEADER_STATUS]);

  return headers;
}

function processRespondWithFD(self, fd, headers, offset = 0, length = -1,
                              streamOptions = 0) {
  const state = self[kState];
  state.flags |= STREAM_FLAGS_HEADERS_SENT;

  const headersList = mapToHeaders(headers, assertValidPseudoHeaderResponse);
  self[kSentHeaders] = headers;
  if (!Array.isArray(headersList)) {
    self.destroy(headersList);
    return;
  }


  // Close the writable side of the stream
  self.end();

  const ret = self[kHandle].respondFD(fd, headersList,
                                      offset, length,
                                      streamOptions);

  if (ret < 0) {
    self.destroy(new NghttpError(ret));
    return;
  }
  // exact length of the file doesn't matter here, since the
  // stream is closing anyway — just use 1 to signify that
  // a write does exist
  trackWriteState(self, 1);
}

function doSendFD(session, options, fd, headers, streamOptions, err, stat) {
  if (err) {
    this.destroy(err);
    return;
  }

  // This can happen if the stream is destroyed or closed while we are waiting
  // for the file descriptor to be opened or the stat call to be completed.
  // In either case, we do not want to continue because the we are shutting
  // down and should not attempt to send any data.
  if (this.destroyed || this.closed) {
    this.destroy(new errors.Error('ERR_HTTP2_INVALID_STREAM'));
    return;
  }

  const statOptions = {
    offset: options.offset !== undefined ? options.offset : 0,
    length: options.length !== undefined ? options.length : -1
  };

  // options.statCheck is a user-provided function that can be used to
  // verify stat values, override or set headers, or even cancel the
  // response operation. If statCheck explicitly returns false, the
  // response is canceled. The user code may also send a separate type
  // of response so check again for the HEADERS_SENT flag
  if ((typeof options.statCheck === 'function' &&
       options.statCheck.call(this, stat, headers, statOptions) === false) ||
       (this[kState].flags & STREAM_FLAGS_HEADERS_SENT)) {
    return;
  }

  processRespondWithFD(this, fd, headers,
                       statOptions.offset | 0,
                       statOptions.length | 0,
                       streamOptions);
}

function doSendFileFD(session, options, fd, headers, streamOptions, err, stat) {
  const onError = options.onError;

  if (err) {
    tryClose(fd);
    if (onError)
      onError(err);
    else
      this.destroy(err);
    return;
  }

  if (!stat.isFile()) {
    const err = new errors.Error('ERR_HTTP2_SEND_FILE');
    if (onError)
      onError(err);
    else
      this.destroy(err);
    return;
  }

  if (this.destroyed || this.closed) {
    tryClose(fd);
    this.destroy(new errors.Error('ERR_HTTP2_INVALID_STREAM'));
    return;
  }

  const statOptions = {
    offset: options.offset !== undefined ? options.offset : 0,
    length: options.length !== undefined ? options.length : -1
  };

  // options.statCheck is a user-provided function that can be used to
  // verify stat values, override or set headers, or even cancel the
  // response operation. If statCheck explicitly returns false, the
  // response is canceled. The user code may also send a separate type
  // of response so check again for the HEADERS_SENT flag
  if ((typeof options.statCheck === 'function' &&
       options.statCheck.call(this, stat, headers) === false) ||
       (this[kState].flags & STREAM_FLAGS_HEADERS_SENT)) {
    return;
  }

  statOptions.length =
    statOptions.length < 0 ? stat.size - (+statOptions.offset) :
      Math.min(stat.size - (+statOptions.offset),
               statOptions.length);

  headers[HTTP2_HEADER_CONTENT_LENGTH] = statOptions.length;

  processRespondWithFD(this, fd, headers,
                       options.offset | 0,
                       statOptions.length | 0,
                       streamOptions);
}

function afterOpen(session, options, headers, streamOptions, err, fd) {
  const state = this[kState];
  const onError = options.onError;
  if (err) {
    if (onError)
      onError(err);
    else
      this.destroy(err);
    return;
  }
  if (this.destroyed || this.closed) {
    tryClose(fd);
    abort(this);
    return;
  }
  state.fd = fd;

  fs.fstat(fd,
           doSendFileFD.bind(this, session, options, fd,
                             headers, streamOptions));
}

function streamOnError(err) {
  // we swallow the error for parity with HTTP1
  // all the errors that ends here are not critical for the project
}


class ServerHttp2Stream extends Http2Stream {
  constructor(session, handle, id, options, headers) {
    super(session, options);
    this[kInit](id, handle);
    this[kProtocol] = headers[HTTP2_HEADER_SCHEME];
    this[kAuthority] = headers[HTTP2_HEADER_AUTHORITY];
    this.on('error', streamOnError);
  }

  // true if the remote peer accepts push streams
  get pushAllowed() {
    return !this.destroyed &&
           !this.closed &&
           !this.session.closed &&
           !this.session.destroyed &&
           this[kSession].remoteSettings.enablePush;
  }

  // create a push stream, call the given callback with the created
  // Http2Stream for the push stream.
  pushStream(headers, options, callback) {
    if (!this.pushAllowed)
      throw new errors.Error('ERR_HTTP2_PUSH_DISABLED');

    const session = this[kSession];

    debug(`Http2Stream ${this[kID]} [Http2Session ` +
          `${sessionName(session[kType])}]: initiating push stream`);

    this[kUpdateTimer]();

    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    if (typeof callback !== 'function')
      throw new errors.TypeError('ERR_INVALID_CALLBACK');

    assertIsObject(options, 'options');
    options = Object.assign({}, options);
    options.endStream = !!options.endStream;

    assertIsObject(headers, 'headers');
    headers = Object.assign(Object.create(null), headers);

    if (headers[HTTP2_HEADER_METHOD] === undefined)
      headers[HTTP2_HEADER_METHOD] = HTTP2_METHOD_GET;
    if (headers[HTTP2_HEADER_AUTHORITY] === undefined)
      headers[HTTP2_HEADER_AUTHORITY] = this[kAuthority];
    if (headers[HTTP2_HEADER_SCHEME] === undefined)
      headers[HTTP2_HEADER_SCHEME] = this[kProtocol];
    if (headers[HTTP2_HEADER_PATH] === undefined)
      headers[HTTP2_HEADER_PATH] = '/';

    let headRequest = false;
    if (headers[HTTP2_HEADER_METHOD] === HTTP2_METHOD_HEAD)
      headRequest = options.endStream = true;
    options.readable = !options.endStream;

    const headersList = mapToHeaders(headers);
    if (!Array.isArray(headersList))
      throw headersList;

    const streamOptions = options.endStream ? STREAM_OPTION_EMPTY_PAYLOAD : 0;

    const ret = this[kHandle].pushPromise(headersList, streamOptions);
    let err;
    if (typeof ret === 'number') {
      switch (ret) {
        case NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE:
          err = new errors.Error('ERR_HTTP2_OUT_OF_STREAMS');
          break;
        case NGHTTP2_ERR_STREAM_CLOSED:
          err = new errors.Error('ERR_HTTP2_INVALID_STREAM');
          break;
        default:
          err = new NghttpError(ret);
          break;
      }
      process.nextTick(callback, err);
      return;
    }

    const id = ret.id();
    const stream = new ServerHttp2Stream(session, ret, id, options, headers);
    stream[kSentHeaders] = headers;

    if (options.endStream)
      stream.end();

    if (headRequest)
      stream[kState].flags |= STREAM_FLAGS_HEAD_REQUEST;

    process.nextTick(callback, null, stream, headers, 0);
  }

  // Initiate a response on this Http2Stream
  respond(headers, options) {
    if (this.destroyed || this.closed)
      throw new errors.Error('ERR_HTTP2_INVALID_STREAM');
    if (this.headersSent)
      throw new errors.Error('ERR_HTTP2_HEADERS_SENT');

    const state = this[kState];

    assertIsObject(options, 'options');
    options = Object.assign({}, options);

    const session = this[kSession];
    debug(`Http2Stream ${this[kID]} [Http2Session ` +
          `${sessionName(session[kType])}]: initiating response`);
    this[kUpdateTimer]();

    options.endStream = !!options.endStream;

    let streamOptions = 0;
    if (options.endStream)
      streamOptions |= STREAM_OPTION_EMPTY_PAYLOAD;

    if (options.getTrailers !== undefined) {
      if (typeof options.getTrailers !== 'function') {
        throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                   'getTrailers',
                                   options.getTrailers);
      }
      streamOptions |= STREAM_OPTION_GET_TRAILERS;
      state.getTrailers = options.getTrailers;
    }

    headers = processHeaders(headers);
    const statusCode = headers[HTTP2_HEADER_STATUS] |= 0;

    // Payload/DATA frames are not permitted in these cases so set
    // the options.endStream option to true so that the underlying
    // bits do not attempt to send any.
    if (statusCode === HTTP_STATUS_NO_CONTENT ||
        statusCode === HTTP_STATUS_RESET_CONTENT ||
        statusCode === HTTP_STATUS_NOT_MODIFIED ||
        this.headRequest === true) {
      options.endStream = true;
    }

    const headersList = mapToHeaders(headers, assertValidPseudoHeaderResponse);
    if (!Array.isArray(headersList))
      throw headersList;
    this[kSentHeaders] = headers;

    state.flags |= STREAM_FLAGS_HEADERS_SENT;

    // Close the writable side if the endStream option is set
    if (options.endStream)
      this.end();

    const ret = this[kHandle].respond(headersList, streamOptions);
    if (ret < 0)
      this.destroy(new NghttpError(ret));
  }

  // Initiate a response using an open FD. Note that there are fewer
  // protections with this approach. For one, the fd is not validated by
  // default. In respondWithFile, the file is checked to make sure it is a
  // regular file, here the fd is passed directly. If the underlying
  // mechanism is not able to read from the fd, then the stream will be
  // reset with an error code.
  respondWithFD(fd, headers, options) {
    if (this.destroyed || this.closed)
      throw new errors.Error('ERR_HTTP2_INVALID_STREAM');
    if (this.headersSent)
      throw new errors.Error('ERR_HTTP2_HEADERS_SENT');

    const session = this[kSession];

    assertIsObject(options, 'options');
    options = Object.assign({}, options);

    if (options.offset !== undefined && typeof options.offset !== 'number')
      throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                 'offset',
                                 options.offset);

    if (options.length !== undefined && typeof options.length !== 'number')
      throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                 'length',
                                 options.length);

    if (options.statCheck !== undefined &&
        typeof options.statCheck !== 'function') {
      throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                 'statCheck',
                                 options.statCheck);
    }

    let streamOptions = 0;
    if (options.getTrailers !== undefined) {
      if (typeof options.getTrailers !== 'function') {
        throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                   'getTrailers',
                                   options.getTrailers);
      }
      streamOptions |= STREAM_OPTION_GET_TRAILERS;
      this[kState].getTrailers = options.getTrailers;
    }

    if (typeof fd !== 'number')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                                 'fd', 'number');

    debug(`Http2Stream ${this[kID]} [Http2Session ` +
          `${sessionName(session[kType])}]: initiating response`);
    this[kUpdateTimer]();

    headers = processHeaders(headers);
    const statusCode = headers[HTTP2_HEADER_STATUS] |= 0;
    // Payload/DATA frames are not permitted in these cases
    if (statusCode === HTTP_STATUS_NO_CONTENT ||
        statusCode === HTTP_STATUS_RESET_CONTENT ||
        statusCode === HTTP_STATUS_NOT_MODIFIED) {
      throw new errors.Error('ERR_HTTP2_PAYLOAD_FORBIDDEN', statusCode);
    }

    if (options.statCheck !== undefined) {
      fs.fstat(fd,
               doSendFD.bind(this, session, options, fd,
                             headers, streamOptions));
      return;
    }

    processRespondWithFD(this, fd, headers,
                         options.offset,
                         options.length,
                         streamOptions);
  }

  // Initiate a file response on this Http2Stream. The path is passed to
  // fs.open() to acquire the fd with mode 'r', then the fd is passed to
  // fs.fstat(). Assuming fstat is successful, a check is made to ensure
  // that the file is a regular file, then options.statCheck is called,
  // giving the user an opportunity to verify the details and set additional
  // headers. If statCheck returns false, the operation is aborted and no
  // file details are sent.
  respondWithFile(path, headers, options) {
    if (this.destroyed || this.closed)
      throw new errors.Error('ERR_HTTP2_INVALID_STREAM');
    if (this.headersSent)
      throw new errors.Error('ERR_HTTP2_HEADERS_SENT');

    assertIsObject(options, 'options');
    options = Object.assign({}, options);

    if (options.offset !== undefined && typeof options.offset !== 'number')
      throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                 'offset',
                                 options.offset);

    if (options.length !== undefined && typeof options.length !== 'number')
      throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                 'length',
                                 options.length);

    if (options.statCheck !== undefined &&
        typeof options.statCheck !== 'function') {
      throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                 'statCheck',
                                 options.statCheck);
    }

    let streamOptions = 0;
    if (options.getTrailers !== undefined) {
      if (typeof options.getTrailers !== 'function') {
        throw new errors.TypeError('ERR_INVALID_OPT_VALUE',
                                   'getTrailers',
                                   options.getTrailers);
      }
      streamOptions |= STREAM_OPTION_GET_TRAILERS;
      this[kState].getTrailers = options.getTrailers;
    }

    const session = this[kSession];
    debug(`Http2Stream ${this[kID]} [Http2Session ` +
          `${sessionName(session[kType])}]: initiating response`);
    this[kUpdateTimer]();


    headers = processHeaders(headers);
    const statusCode = headers[HTTP2_HEADER_STATUS] |= 0;
    // Payload/DATA frames are not permitted in these cases
    if (statusCode === HTTP_STATUS_NO_CONTENT ||
        statusCode === HTTP_STATUS_RESET_CONTENT ||
        statusCode === HTTP_STATUS_NOT_MODIFIED) {
      throw new errors.Error('ERR_HTTP2_PAYLOAD_FORBIDDEN', statusCode);
    }

    fs.open(path, 'r',
            afterOpen.bind(this, session, options, headers, streamOptions));
  }

  // Sends a block of informational headers. In theory, the HTTP/2 spec
  // allows sending a HEADER block at any time during a streams lifecycle,
  // but the HTTP request/response semantics defined in HTTP/2 places limits
  // such that HEADERS may only be sent *before* or *after* DATA frames.
  // If the block of headers being sent includes a status code, it MUST be
  // a 1xx informational code and it MUST be sent before the request/response
  // headers are sent, or an error will be thrown.
  additionalHeaders(headers) {
    if (this.destroyed || this.closed)
      throw new errors.Error('ERR_HTTP2_INVALID_STREAM');
    if (this.headersSent)
      throw new errors.Error('ERR_HTTP2_HEADERS_AFTER_RESPOND');

    assertIsObject(headers, 'headers');
    headers = Object.assign(Object.create(null), headers);

    const session = this[kSession];
    debug(`Http2Stream ${this[kID]} [Http2Session ` +
    `${sessionName(session[kType])}]: sending additional headers`);

    if (headers[HTTP2_HEADER_STATUS] != null) {
      const statusCode = headers[HTTP2_HEADER_STATUS] |= 0;
      if (statusCode === HTTP_STATUS_SWITCHING_PROTOCOLS)
        throw new errors.Error('ERR_HTTP2_STATUS_101');
      if (statusCode < 100 || statusCode >= 200) {
        throw new errors.RangeError('ERR_HTTP2_INVALID_INFO_STATUS',
                                    headers[HTTP2_HEADER_STATUS]);
      }
    }

    this[kUpdateTimer]();

    const headersList = mapToHeaders(headers, assertValidPseudoHeaderResponse);
    if (!Array.isArray(headersList))
      throw headersList;
    if (!this[kInfoHeaders])
      this[kInfoHeaders] = [headers];
    else
      this[kInfoHeaders].push(headers);

    const ret = this[kHandle].info(headersList);
    if (ret < 0)
      this.destroy(new NghttpError(ret));
  }
}

ServerHttp2Stream.prototype[kProceed] = ServerHttp2Stream.prototype.respond;

class ClientHttp2Stream extends Http2Stream {
  constructor(session, handle, id, options) {
    super(session, options);
    this[kState].flags |= STREAM_FLAGS_HEADERS_SENT;
    if (id !== undefined)
      this[kInit](id, handle);
    this.on('headers', handleHeaderContinue);
  }
}

function handleHeaderContinue(headers) {
  if (headers[HTTP2_HEADER_STATUS] === HTTP_STATUS_CONTINUE)
    this.emit('continue');
}

const setTimeout = {
  configurable: true,
  enumerable: true,
  writable: true,
  value: function(msecs, callback) {
    if (this.destroyed)
      return;

    // Type checking identical to timers.enroll()
    msecs = validateTimerDuration(msecs);

    // Attempt to clear an existing timer lear in both cases -
    //  even if it will be rescheduled we don't want to leak an existing timer.
    clearTimeout(this[kTimeout]);

    if (msecs === 0) {
      if (callback !== undefined) {
        if (typeof callback !== 'function')
          throw new errors.TypeError('ERR_INVALID_CALLBACK');
        this.removeListener('timeout', callback);
      }
    } else {
      this[kTimeout] = setUnrefTimeout(this._onTimeout.bind(this), msecs);
      if (this[kSession]) this[kSession][kUpdateTimer]();

      if (callback !== undefined) {
        if (typeof callback !== 'function')
          throw new errors.TypeError('ERR_INVALID_CALLBACK');
        this.once('timeout', callback);
      }
    }
    return this;
  }
};
Object.defineProperty(Http2Stream.prototype, 'setTimeout', setTimeout);
Object.defineProperty(Http2Session.prototype, 'setTimeout', setTimeout);


// When the socket emits an error, destroy the associated Http2Session and
// forward it the same error.
function socketOnError(error) {
  const session = this[kSession];
  if (session !== undefined) {
    debug(`Http2Session ${sessionName(session[kType])}: socket error [` +
          `${error.message}]`);
    session.destroy(error);
  }
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
  if (this[kServer])
    this[kServer].emit('sessionError', error, this);
}

// When the session times out on the server, try emitting a timeout event.
// If no handler is registered, destroy the session.
function sessionOnTimeout() {
  // if destroyed or closed already, do nothing
  if (this.destroyed || this.closed)
    return;
  const server = this[kServer];
  if (!server.emit('timeout', this))
    this.destroy();  // No error code, just things down.
}

function connectionListener(socket) {
  debug('Http2Session server: received a connection');
  const options = this[kOptions] || {};

  if (socket.alpnProtocol === false || socket.alpnProtocol === 'http/1.1') {
    // Fallback to HTTP/1.1
    if (options.allowHTTP1 === true)
      return httpConnectionListener.call(this, socket);
    // Let event handler deal with the socket
    if (!this.emit('unknownProtocol', socket))
      socket.destroy();
    return;
  }

  socket.on('error', socketOnError);
  socket.on('close', socketOnClose);

  // Set up the Session
  const session = new ServerHttp2Session(options, socket, this);

  session.on('stream', sessionOnStream);
  session.on('priority', sessionOnPriority);
  session.on('error', sessionOnError);

  if (this.timeout)
    session.setTimeout(this.timeout, sessionOnTimeout);

  socket[kServer] = this;

  this.emit('session', session);
}

function initializeOptions(options) {
  assertIsObject(options, 'options');
  options = Object.assign({}, options);
  options.allowHalfOpen = true;
  assertIsObject(options.settings, 'options.settings');
  options.settings = Object.assign({}, options.settings);
  return options;
}

function initializeTLSOptions(options, servername) {
  options = initializeOptions(options);
  options.ALPNProtocols = ['h2'];
  if (options.allowHTTP1 === true)
    options.ALPNProtocols.push('http/1.1');
  if (servername !== undefined && options.servername === undefined)
    options.servername = servername;
  return options;
}

function onErrorSecureServerSession(err, socket) {
  if (!this.emit('clientError', err, socket))
    socket.destroy(err);
}

class Http2SecureServer extends TLSServer {
  constructor(options, requestListener) {
    options = initializeTLSOptions(options);
    super(options, connectionListener);
    this[kOptions] = options;
    this.timeout = kDefaultSocketTimeout;
    this.on('newListener', setupCompat);
    if (typeof requestListener === 'function')
      this.on('request', requestListener);
    this.on('tlsClientError', onErrorSecureServerSession);
  }

  setTimeout(msecs, callback) {
    this.timeout = msecs;
    if (callback !== undefined) {
      if (typeof callback !== 'function')
        throw new errors.TypeError('ERR_INVALID_CALLBACK');
      this.on('timeout', callback);
    }
    return this;
  }
}

class Http2Server extends NETServer {
  constructor(options, requestListener) {
    super(connectionListener);
    this[kOptions] = initializeOptions(options);
    this.timeout = kDefaultSocketTimeout;
    this.on('newListener', setupCompat);
    if (typeof requestListener === 'function')
      this.on('request', requestListener);
  }

  setTimeout(msecs, callback) {
    this.timeout = msecs;
    if (callback !== undefined) {
      if (typeof callback !== 'function')
        throw new errors.TypeError('ERR_INVALID_CALLBACK');
      this.on('timeout', callback);
    }
    return this;
  }
}

function setupCompat(ev) {
  if (ev === 'request') {
    this.removeListener('newListener', setupCompat);
    this.on('stream', onServerStream);
  }
}

function socketOnClose() {
  const session = this[kSession];
  if (session !== undefined) {
    debug(`Http2Session ${sessionName(session[kType])}: socket closed`);
    const err = session.connecting ?
      new errors.Error('ERR_SOCKET_CLOSED') : null;
    const state = session[kState];
    state.streams.forEach((stream) => stream.close(NGHTTP2_CANCEL));
    state.pendingStreams.forEach((stream) => stream.close(NGHTTP2_CANCEL));
    session.close();
    session[kMaybeDestroy](err);
  }
}

function connect(authority, options, listener) {
  if (typeof options === 'function') {
    listener = options;
    options = undefined;
  }

  assertIsObject(options, 'options');
  options = Object.assign({}, options);

  if (typeof authority === 'string')
    authority = new URL(authority);

  assertIsObject(authority, 'authority', ['string', 'Object', 'URL']);

  const protocol = authority.protocol || options.protocol || 'https:';
  const port = '' + (authority.port !== '' ?
    authority.port : (authority.protocol === 'http:' ? 80 : 443));
  const host = authority.hostname || authority.host || 'localhost';

  let socket;
  if (typeof options.createConnection === 'function') {
    socket = options.createConnection(authority, options);
  } else {
    switch (protocol) {
      case 'http:':
        socket = net.connect(port, host);
        break;
      case 'https:':
        socket = tls.connect(port, host, initializeTLSOptions(options, host));
        break;
      default:
        throw new errors.Error('ERR_HTTP2_UNSUPPORTED_PROTOCOL', protocol);
    }
  }

  socket.on('error', socketOnError);
  socket.on('close', socketOnClose);

  const session = new ClientHttp2Session(options, socket);

  session[kAuthority] = `${options.servername || host}:${port}`;
  session[kProtocol] = protocol;

  if (typeof listener === 'function')
    session.once('connect', listener);
  return session;
}

// Support util.promisify
Object.defineProperty(connect, promisify.custom, {
  value: (authority, options) => {
    const promise = createPromise();
    const server = connect(authority,
                           options,
                           () => promiseResolve(promise, server));
    return promise;
  }
});

function createSecureServer(options, handler) {
  assertIsObject(options, 'options');
  return new Http2SecureServer(options, handler);
}

function createServer(options, handler) {
  if (typeof options === 'function') {
    handler = options;
    options = {};
  }
  assertIsObject(options, 'options');
  return new Http2Server(options, handler);
}

// Returns a Base64 encoded settings frame payload from the given
// object. The value is suitable for passing as the value of the
// HTTP2-Settings header frame.
function getPackedSettings(settings) {
  assertIsObject(settings, 'settings');
  updateSettingsBuffer(validateSettings(settings));
  return binding.packSettings();
}

function getUnpackedSettings(buf, options = {}) {
  if (!isArrayBufferView(buf)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'buf',
                               ['Buffer', 'TypedArray', 'DataView']);
  }
  if (buf.length % 6 !== 0)
    throw new errors.RangeError('ERR_HTTP2_INVALID_PACKED_SETTINGS_LENGTH');
  const settings = {};
  let offset = 0;
  while (offset < buf.length) {
    const id = buf.readUInt16BE(offset);
    offset += 2;
    const value = buf.readUInt32BE(offset);
    switch (id) {
      case NGHTTP2_SETTINGS_HEADER_TABLE_SIZE:
        settings.headerTableSize = value;
        break;
      case NGHTTP2_SETTINGS_ENABLE_PUSH:
        settings.enablePush = value !== 0;
        break;
      case NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS:
        settings.maxConcurrentStreams = value;
        break;
      case NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE:
        settings.initialWindowSize = value;
        break;
      case NGHTTP2_SETTINGS_MAX_FRAME_SIZE:
        settings.maxFrameSize = value;
        break;
      case NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE:
        settings.maxHeaderListSize = value;
        break;
    }
    offset += 4;
  }

  if (options != null && options.validate)
    validateSettings(settings);

  return settings;
}

// Exports
module.exports = {
  constants,
  getDefaultSettings,
  getPackedSettings,
  getUnpackedSettings,
  createServer,
  createSecureServer,
  connect,
  Http2Session,
  Http2Stream,
  Http2ServerRequest,
  Http2ServerResponse
};

/* eslint-enable no-use-before-define */
