'use strict';

const {
  ObjectDefineProperty,
  Promise,
  Symbol,
} = primordials;

const {
  assertCrypto,
  promisify
} = require('internal/util');

assertCrypto();

const EventEmitter = require('events');
const http = require('http');
const net = require('net');
const tls = require('tls');
const { URL } = require('url');

const {
  kIncomingMessage,
} = require('_http_common');
const { kServerResponse } = require('_http_server');

const {
  codes: {
    ERR_HTTP2_INVALID_PACKED_SETTINGS_LENGTH,
    ERR_HTTP2_UNSUPPORTED_PROTOCOL,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_CALLBACK,
    ERR_SOCKET_CLOSED
  },
} = require('internal/errors');
const { validateUint32 } = require('internal/validators');
const { onServerStream,
        Http2ServerRequest,
        Http2ServerResponse,
} = require('internal/http2/compat');
const {
  kAuthority,
  kNativeFields,
  kProtocol,
  onStreamClose,
  onStreamTrailers,
  Http2Stream,
} = require('internal/http2/streams');
const {
  constants: {
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
} = require('internal/http2/session');

const {
  assertIsObject,
  getDefaultSettings,
  kSensitiveHeaders,
  kState,
  updateSettingsBuffer,
  validateSettings,
} = require('internal/http2/util');
const {
  kMaybeDestroy,
  kSession,
} = require('internal/stream_base_commons');
const { isArrayBufferView } = require('internal/util/types');

const binding = internalBinding('http2');

const { _connectionListener: httpConnectionListener } = http;
let debug = require('internal/util/debuglog').debuglog('http2', (fn) => {
  debug = fn;
});

const {
  constants,
  kSessionPriorityListenerCount,
} = binding;

const NETServer = net.Server;
const TLSServer = tls.Server;

const kOptions = Symbol('options');

const {
  NGHTTP2_CANCEL,
  HTTP2_HEADER_STATUS,

  NGHTTP2_SETTINGS_HEADER_TABLE_SIZE,
  NGHTTP2_SETTINGS_ENABLE_PUSH,
  NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS,
  NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE,
  NGHTTP2_SETTINGS_MAX_FRAME_SIZE,
  NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE,
  NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL,
} = constants;

// When the socket emits an error, destroy the associated Http2Session and
// forward it the same error.
function socketOnError(error) {
  const session = this[kSession];
  if (session !== undefined) {
    // We can ignore ECONNRESET after GOAWAY was received as there's nothing
    // we can do and the other side is fully within its rights to do so.
    if (error.code === 'ECONNRESET' && session[kState].goawayCode !== null)
      return session.destroy();
    debugSessionObj(this, 'socket error [%s]', error.message);
    session.destroy(error);
  }
}

function connectionListener(socket) {
  debug('Http2Session server: received a connection');
  const options = this[kOptions] || {};

  if (socket.alpnProtocol === false || socket.alpnProtocol === 'http/1.1') {
    // Fallback to HTTP/1.1
    if (options.allowHTTP1 === true) {
      socket.server[kIncomingMessage] = options.Http1IncomingMessage;
      socket.server[kServerResponse] = options.Http1ServerResponse;
      return httpConnectionListener.call(this, socket);
    }
    // Let event handler deal with the socket
    debug('Unknown protocol from %s:%s',
          socket.remoteAddress, socket.remotePort);
    if (!this.emit('unknownProtocol', socket)) {
      // We don't know what to do, so let's just tell the other side what's
      // going on in a format that they *might* understand.
      socket.end('HTTP/1.0 403 Forbidden\r\n' +
                 'Content-Type: text/plain\r\n\r\n' +
                 'Unknown ALPN Protocol, expected `h2` to be available.\n' +
                 'If this is a HTTP request: The server was not ' +
                 'configured with the `allowHTTP1` option or a ' +
                 'listener for the `unknownProtocol` event.\n');
    }
    return;
  }

  socket.on('error', socketOnError);
  socket.on('close', socketOnClose);

  // Set up the Session
  const session = new ServerHttp2Session(options, socket, this);

  session.on('stream', sessionOnStream);
  session.on('error', sessionOnError);
  // Don't count our own internal listener.
  session.on('priority', sessionOnPriority);
  session[kNativeFields][kSessionPriorityListenerCount]--;

  if (this.timeout)
    session.setTimeout(this.timeout, sessionOnTimeout);

  socket[kServer] = this;

  this.emit('session', session);
}

function initializeOptions(options) {
  assertIsObject(options, 'options');
  options = { ...options };
  assertIsObject(options.settings, 'options.settings');
  options.settings = { ...options.settings };

  if (options.maxSessionInvalidFrames !== undefined)
    validateUint32(options.maxSessionInvalidFrames, 'maxSessionInvalidFrames');

  if (options.maxSessionRejectedStreams !== undefined) {
    validateUint32(
      options.maxSessionRejectedStreams,
      'maxSessionRejectedStreams'
    );
  }

  // Used only with allowHTTP1
  options.Http1IncomingMessage = options.Http1IncomingMessage ||
    http.IncomingMessage;
  options.Http1ServerResponse = options.Http1ServerResponse ||
    http.ServerResponse;

  options.Http2ServerRequest = options.Http2ServerRequest ||
                                       Http2ServerRequest;
  options.Http2ServerResponse = options.Http2ServerResponse ||
                                        Http2ServerResponse;
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
    this.timeout = 0;
    this.on('newListener', setupCompat);
    if (typeof requestListener === 'function')
      this.on('request', requestListener);
    this.on('tlsClientError', onErrorSecureServerSession);
  }

  setTimeout(msecs, callback) {
    this.timeout = msecs;
    if (callback !== undefined) {
      if (typeof callback !== 'function')
        throw new ERR_INVALID_CALLBACK(callback);
      this.on('timeout', callback);
    }
    return this;
  }
}

class Http2Server extends NETServer {
  constructor(options, requestListener) {
    options = initializeOptions(options);
    super(options, connectionListener);
    this[kOptions] = options;
    this.timeout = 0;
    this.on('newListener', setupCompat);
    if (typeof requestListener === 'function')
      this.on('request', requestListener);
  }

  setTimeout(msecs, callback) {
    this.timeout = msecs;
    if (callback !== undefined) {
      if (typeof callback !== 'function')
        throw new ERR_INVALID_CALLBACK(callback);
      this.on('timeout', callback);
    }
    return this;
  }
}

Http2Server.prototype[EventEmitter.captureRejectionSymbol] = function(
  err, event, ...args) {

  switch (event) {
    case 'stream':
      // TODO(mcollina): we might want to match this with what we do on
      // the compat side.
      const [stream] = args;
      if (stream.sentHeaders) {
        stream.destroy(err);
      } else {
        stream.respond({ [HTTP2_HEADER_STATUS]: 500 });
        stream.end();
      }
      break;
    case 'request':
      const [, res] = args;
      if (!res.headersSent && !res.finished) {
        // Don't leak headers.
        for (const name of res.getHeaderNames()) {
          res.removeHeader(name);
        }
        res.statusCode = 500;
        res.end(http.STATUS_CODES[500]);
      } else {
        res.destroy();
      }
      break;
    default:
      net.Server.prototype[EventEmitter.captureRejectionSymbol]
        .call(this, err, event, ...args);
  }
};

function setupCompat(ev) {
  if (ev === 'request') {
    this.removeListener('newListener', setupCompat);
    this.on('stream', onServerStream.bind(
      this,
      this[kOptions].Http2ServerRequest,
      this[kOptions].Http2ServerResponse)
    );
  }
}

function socketOnClose() {
  const session = this[kSession];
  if (session !== undefined) {
    debugSessionObj(session, 'socket closed');
    const err = session.connecting ? new ERR_SOCKET_CLOSED() : null;
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
  options = { ...options };

  if (typeof authority === 'string')
    authority = new URL(authority);

  assertIsObject(authority, 'authority', ['string', 'Object', 'URL']);

  const protocol = authority.protocol || options.protocol || 'https:';
  const port = '' + (authority.port !== '' ?
    authority.port : (authority.protocol === 'http:' ? 80 : 443));
  let host = 'localhost';

  if (authority.hostname) {
    host = authority.hostname;

    if (host[0] === '[')
      host = host.slice(1, -1);
  } else if (authority.host) {
    host = authority.host;
  }

  let socket;
  if (typeof options.createConnection === 'function') {
    socket = options.createConnection(authority, options);
  } else {
    switch (protocol) {
      case 'http:':
        socket = net.connect({ port, host, ...options });
        break;
      case 'https:':
        socket = tls.connect(port, host, initializeTLSOptions(options, host));
        break;
      default:
        throw new ERR_HTTP2_UNSUPPORTED_PROTOCOL(protocol);
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
ObjectDefineProperty(connect, promisify.custom, {
  value: (authority, options) => {
    return new Promise((resolve) => {
      const server = connect(authority, options, () => resolve(server));
    });
  }
});

function createSecureServer(options, handler) {
  return new Http2SecureServer(options, handler);
}

function createServer(options, handler) {
  if (typeof options === 'function') {
    handler = options;
    options = {};
  }
  return new Http2Server(options, handler);
}

// Returns a Base64 encoded settings frame payload from the given
// object. The value is suitable for passing as the value of the
// HTTP2-Settings header frame.
function getPackedSettings(settings) {
  assertIsObject(settings, 'settings');
  validateSettings(settings);
  updateSettingsBuffer({ ...settings });
  return binding.packSettings();
}

function getUnpackedSettings(buf, options = {}) {
  if (!isArrayBufferView(buf)) {
    throw new ERR_INVALID_ARG_TYPE('buf',
                                   ['Buffer', 'TypedArray', 'DataView'], buf);
  }
  if (buf.length % 6 !== 0)
    throw new ERR_HTTP2_INVALID_PACKED_SETTINGS_LENGTH();
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
      case NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL:
        settings.enableConnectProtocol = value !== 0;
    }
    offset += 4;
  }

  if (options != null && options.validate)
    validateSettings(settings);

  return settings;
}

binding.setCallbackFunctions(
  onSessionInternalError,
  onPriority,
  onSettings,
  onPing,
  onSessionHeaders,
  onFrameError,
  onGoawayData,
  onAltSvc,
  onOrigin,
  onStreamTrailers,
  onStreamClose
);

// Exports
module.exports = {
  connect,
  constants,
  createServer,
  createSecureServer,
  getDefaultSettings,
  getPackedSettings,
  getUnpackedSettings,
  sensitiveHeaders: kSensitiveHeaders,
  Http2Session,
  Http2Stream,
  Http2ServerRequest,
  Http2ServerResponse
};
