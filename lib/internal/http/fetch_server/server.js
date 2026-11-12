'use strict';

// The infrastructure layer of the Fetch Server API: serve(), Server, and
// Listener (Section 17, Section 18).
// https://fetch-server.proposal.wintertc.org/#server-configuration

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
  FunctionPrototypeCall,
  ObjectDefineProperties,
  ObjectIs,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  RegExpPrototypeExec,
  SafeArrayIterator,
  SafeSet,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  StringPrototypeToLowerCase,
  SymbolAsyncDispose,
  SymbolFor,
  SymbolIterator,
  SymbolToStringTag,
  TypedArrayPrototypeGetByteLength,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  customInspectSymbol: kInspect,
  kEnumerableProperty,
  lazyDOMException,
} = require('internal/util');

const { inspect } = require('internal/util/inspect');

const {
  converters,
} = require('internal/webidl');

const {
  invalidState,
  notSupported,
} = require('internal/http/fetch_server/errors');

const {
  markHandled,
  noop,
} = require('internal/http/fetch_server/body');

const { Exchange, report } = require('internal/http/fetch_server/exchange');
const { dispatch } = require('internal/http/fetch_server/dispatch');
const { createServerRequest } = require('internal/http/fetch_server/request');
const { H1Backend } = require('internal/http/fetch_server/h1');
const { H2Backend } = require('internal/http/fetch_server/h2');

const kProtocolMarker = SymbolFor('wintertc.server.protocol');
const kH2Preface = 'PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n';
const kUpgradeConnection = /(?:^|,)\s*upgrade\s*(?:,|$)/i;

// =============================================================================
// Option conversion
// =============================================================================

/**
 * Converts a handler object (Section 16.5).
 * @param {any} handler
 * @returns {{ fetch?: Function, connect?: Function, error?: Function,
 *   object: object }}
 */
function convertHandler(handler) {
  if (handler === null ||
      (typeof handler !== 'object' && typeof handler !== 'function')) {
    throw new ERR_INVALID_ARG_TYPE('handler', 'Object', handler);
  }
  const marker = handler[kProtocolMarker];
  if (marker !== undefined && !ObjectIs(marker, 1)) {
    throw new ERR_INVALID_ARG_VALUE(
      'handler[Symbol.for(\'wintertc.server.protocol\')]', marker,
      'is not a supported protocol version');
  }
  const dict = { __proto__: null, object: handler };
  for (const name of new SafeArrayIterator(['connect', 'error', 'fetch'])) {
    const value = handler[name];
    if (value === undefined) continue;
    if (typeof value !== 'function') {
      throw new ERR_INVALID_ARG_TYPE(`handler.${name}`, 'Function', value);
    }
    dict[name] = value;
  }
  if (dict.fetch === undefined && dict.connect === undefined) {
    throw new ERR_INVALID_ARG_VALUE('handler', handler,
                                    'must have a fetch or connect method');
  }
  return dict;
}

function convertInteger(value, name, type = 'unsigned long long') {
  if (value === undefined) return undefined;
  return converters[type === 'unsigned long long' ? 'long long' : type](
    value, { __proto__: null, enforceRange: true, context: name });
}

function convertNonNegative(value, name) {
  const n = convertInteger(value, name);
  if (n !== undefined && n < 0) {
    throw new ERR_INVALID_ARG_VALUE(name, value, 'must be non-negative');
  }
  return n;
}

function convertDictionary(value, name) {
  if (value === undefined || value === null) return undefined;
  if (typeof value !== 'object' && typeof value !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(name, 'Object', value);
  }
  return value;
}

const kTimeoutMembers = [
  'closeHandshake', 'handshake', 'headers', 'idle', 'receive', 'send',
  'tunnelIdle', 'waitUntil',
];
const kLimitMembers = [
  'maxConcurrentStreams', 'maxConnections', 'maxHeaderSize',
  'maxRequestBodySize', 'maxRequestsPerConnection',
];

function convertMembers(value, name, members) {
  const dict = convertDictionary(value, name);
  if (dict === undefined) return undefined;
  const result = { __proto__: null };
  for (let i = 0; i < members.length; i++) {
    const v = convertNonNegative(dict[members[i]], `${name}.${members[i]}`);
    if (v !== undefined) result[members[i]] = v;
  }
  return result;
}

function convertTLS(value) {
  const dict = convertDictionary(value, 'tls');
  if (dict === undefined) return undefined;
  const result = { __proto__: null };
  const alpn = dict.alpn;
  if (alpn !== undefined) {
    result.alpn = converters['sequence<DOMString>'](alpn, {
      __proto__: null, context: 'tls.alpn',
    });
  }
  if (dict.cert !== undefined) result.cert = dict.cert;
  if (dict.key !== undefined) result.key = dict.key;
  const sni = dict.sni;
  if (sni !== undefined) {
    if (typeof sni !== 'function' &&
        (sni === null || typeof sni !== 'object')) {
      throw new ERR_INVALID_ARG_TYPE('tls.sni', ['Function', 'Object'], sni);
    }
    result.sni = sni;
  }
  return result;
}

function convertBooleanOrUndefined(value) {
  return value === undefined ? undefined : !!value;
}

/**
 * Converts the members shared by ServerOptions and ListenOptions.
 * @param {object} options
 * @returns {object}
 */
function convertListenMembers(options) {
  const result = { __proto__: null };
  const altSvc = options.altSvc;
  if (altSvc !== undefined) {
    result.altSvc = typeof altSvc === 'boolean' ? altSvc :
      converters.DOMString(altSvc, { __proto__: null, context: 'altSvc' });
  }
  result.backlog = convertInteger(options.backlog, 'backlog', 'unsigned long');
  result.datagrams = convertBooleanOrUndefined(options.datagrams);
  result.extendedConnect = convertBooleanOrUndefined(options.extendedConnect);
  if (options.hostname !== undefined) {
    result.hostname = converters.DOMString(options.hostname, {
      __proto__: null, context: 'hostname',
    });
  }
  result.ipv6Only = convertBooleanOrUndefined(options.ipv6Only);
  result.limits = convertMembers(options.limits, 'limits', kLimitMembers);
  result.port = convertInteger(options.port, 'port', 'unsigned short');
  const quic = options.quic;
  if (quic !== undefined) {
    result.quic = typeof quic === 'boolean' ? quic :
      (convertDictionary(quic, 'quic') !== undefined);
  }
  result.reusePort = convertBooleanOrUndefined(options.reusePort);
  result.timeouts = convertMembers(options.timeouts, 'timeouts',
                                   kTimeoutMembers);
  result.tls = convertTLS(options.tls);
  result.webTransport = convertBooleanOrUndefined(options.webTransport);
  return result;
}

function convertServerOptions(options) {
  options ??= {};
  if (typeof options !== 'object' && typeof options !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
  }
  const result = convertListenMembers(options);
  result.autoContinue = options.autoContinue === undefined ? true :
    !!options.autoContinue;
  const signal = options.signal;
  if (signal !== undefined) {
    const { validateAbortSignal } = require('internal/validators');
    validateAbortSignal(signal, 'options.signal');
    result.signal = signal;
  }
  return result;
}

/**
 * Determines the configuration of a binding member by member (Section 17.2).
 * @returns {object}
 */
function resolveListenOptions(serverOptions, listenOptions) {
  const result = { __proto__: null };
  const keys = ['altSvc', 'backlog', 'datagrams', 'extendedConnect', 'hostname',
                'ipv6Only', 'port', 'quic', 'reusePort', 'tls', 'webTransport'];
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    result[key] = listenOptions[key] !== undefined ? listenOptions[key] :
      serverOptions[key];
  }
  result.timeouts = {
    __proto__: null, ...serverOptions.timeouts, ...listenOptions.timeouts,
  };
  result.limits = {
    __proto__: null, ...serverOptions.limits, ...listenOptions.limits,
  };
  result.port ??= 0;
  result.reusePort ??= false;
  result.ipv6Only ??= false;
  result.quic ??= false;
  return result;
}

// =============================================================================
// Closeable (Section 18.2)
// =============================================================================

class Closeable {
  #closed = PromiseWithResolvers();
  #closing = false;
  #destroyed = false;
  #done = false;

  constructor() {
    markHandled(this.#closed.promise);
  }

  get closedPromise() {
    return this.#closed.promise;
  }

  get closing() {
    return this.#closing;
  }

  get destroyed() {
    return this.#destroyed;
  }

  get done() {
    return this.#done;
  }

  startClose() {
    if (this.#closing || this.#destroyed) return false;
    this.#closing = true;
    return true;
  }

  startDestroy() {
    if (this.#destroyed || this.#done) return false;
    this.#destroyed = true;
    return true;
  }

  resolveClosed() {
    if (this.#done) return;
    this.#done = true;
    this.#closed.resolve();
  }

  rejectClosed(error) {
    if (this.#done) return;
    this.#done = true;
    this.#closed.reject(error);
  }
}

function abortError() {
  return lazyDOMException('The server was destroyed', 'AbortError');
}

// =============================================================================
// Listener (Section 5.16)
// =============================================================================

let getListenerInternals;
let isListener;

class ListenerInternals extends Closeable {
  server;
  options;
  netServer = null;
  http1 = null;
  http2 = null;
  busy = false;
  sockets = new SafeSet();
  sessions = new SafeSet();
  exchanges = new SafeSet();
  altSvc;
  maxRequestBodySize;
  waitUntilTimeout;

  constructor(server, options) {
    super();
    this.server = server;
    this.options = options;
    this.busy = server.busy;
    this.altSvc = typeof options.altSvc === 'string' ? options.altSvc : undefined;
    this.maxRequestBodySize = options.limits.maxRequestBodySize ?? 0;
    this.waitUntilTimeout = options.timeouts.waitUntil ?? 30_000;
  }

  get draining() {
    return this.closing;
  }

  get destroying() {
    return this.destroyed;
  }
}

class Listener {
  #internals;

  static {
    getListenerInternals = (listener) => listener.#internals;
    isListener = (value) => {
      return value != null && typeof value === 'object' && #internals in value;
    };
  }

  constructor(key, internals) {
    if (key !== kConstruct) {
      throw new ERR_INVALID_THIS('Listener cannot be constructed');
    }
    this.#internals = internals;
  }

  get address() {
    const internals = checkListener(this);
    const address = internals.netServer?.address();
    if (address === null || address === undefined) {
      return { address: '', port: 0 };
    }
    if (typeof address === 'string') {
      return { address: '', port: 0, family: 'unix', path: address };
    }
    return {
      address: address.address,
      port: address.port,
      family: address.family === 'IPv6' ? 'IPv6' : 'IPv4',
    };
  }

  get busy() {
    return checkListener(this).busy;
  }

  set busy(value) {
    checkListener(this).busy = !!value;
  }

  close() {
    const internals = checkListener(this);
    closeListener(internals);
    return internals.closedPromise;
  }

  destroy(error = undefined) {
    destroyListener(checkListener(this), error);
  }

  get closed() {
    return checkListener(this).closedPromise;
  }

  [SymbolAsyncDispose]() {
    return this.close();
  }

  [kInspect](depth, options) {
    if (depth < 0) return 'Listener {}';
    return `Listener ${inspect({ address: this.address, busy: this.busy },
                               options)}`;
  }
}

function checkListener(self) {
  if (!isListener(self)) throw new ERR_INVALID_THIS('Listener');
  return getListenerInternals(self);
}

ObjectDefineProperties(Listener.prototype, {
  address: kEnumerableProperty,
  busy: kEnumerableProperty,
  close: kEnumerableProperty,
  destroy: kEnumerableProperty,
  closed: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'Listener',
  },
});

const kConstruct = {};

function maybeFinishClose(internals) {
  if (!internals.closing || internals.done) return;
  if (internals.netServer?.listening) return;
  if (internals.sockets.size > 0 || internals.exchanges.size > 0) return;
  internals.http1?.close();
  internals.resolveClosed();
  internals.server.listenerClosed(internals);
}

function closeListener(internals) {
  if (!internals.startClose()) return;
  const { netServer } = internals;
  if (netServer?.listening) {
    netServer.close(() => maybeFinishClose(internals));
  }
  for (const session of internals.sessions) {
    try {
      session.close();
    } catch {
      // Ignore sessions that are already closing.
    }
  }
  internals.http1?.closeIdleConnections();
  for (const exchange of internals.exchanges) {
    exchange.tunnel?.windDown?.();
  }
  maybeFinishClose(internals);
}

function destroyListener(internals, error) {
  if (!internals.startDestroy()) return;
  internals.netServer?.close();
  for (const exchange of internals.exchanges) {
    exchange.forceEnd();
  }
  for (const session of internals.sessions) {
    session.destroy();
  }
  for (const socket of internals.sockets) {
    socket.destroy();
  }
  internals.http1?.close();
  internals.rejectClosed(error === undefined ? abortError() : error);
  internals.server.listenerClosed(internals);
}

// =============================================================================
// Exchange creation
// =============================================================================

function createExchange(internals, kind, backend) {
  const server = internals.server;
  const exchange = new Exchange(kind, backend, internals, {
    autoContinue: server.options.autoContinue,
    maxRequestBodySize: internals.maxRequestBodySize,
    waitUntilTimeout: internals.waitUntilTimeout,
    requestFactory: (ex) => createServerRequest(
      ex, backend.method, backend.url, backend.headerList),
    onEnded: (ex) => {
      internals.exchanges.delete(ex);
      maybeFinishClose(internals);
    },
  });
  internals.exchanges.add(exchange);
  backend.attach(exchange);
  return exchange;
}

function refusedByBusy(internals) {
  return internals.busy || internals.closing;
}

function writeSimpleResponse(res, status, extraHeaders = []) {
  res.shouldKeepAlive = false;
  res.writeHead(status, extraHeaders);
  res.end();
}

function onHttp1Request(internals, isContinue, req, res) {
  if (internals.destroyed) {
    req.socket.destroy();
    return;
  }
  if (refusedByBusy(internals)) {
    res.writeHead(503, internals.closing ? ['Connection', 'close'] : []);
    res.end();
    return;
  }
  if (req.url === '*') {
    res.writeHead(204, ['Allow', 'GET, HEAD, POST, PUT, DELETE, OPTIONS']);
    res.end();
    return;
  }
  const limit = internals.maxRequestBodySize;
  if (limit > 0 && +(req.headers['content-length'] ?? 0) > limit) {
    writeSimpleResponse(res, 413);
    return;
  }
  let backend;
  try {
    backend = new H1Backend({
      req, res, socket: req.socket, listener: internals, kind: 'fetch',
    });
  } catch {
    writeSimpleResponse(res, 400);
    return;
  }
  const exchange = createExchange(internals, 'fetch', backend);
  exchange.pending100 = isContinue;
  dispatch(internals.server.handlers, exchange);
}

function createLegacyResponse(internals, req, socket) {
  const { ServerResponse } = require('internal/http/server');
  const res = new ServerResponse(req);
  res.shouldKeepAlive = false;
  res.assignSocket(socket);
  res.on('finish', () => {
    res.detachSocket(socket);
    socket.end();
  });
  return res;
}

function onHttp1Tunnel(internals, kind, req, stream, head) {
  const socket = req.socket;
  internals.sockets.add(socket);
  socket.on('error', noop);
  if (internals.destroyed) {
    socket.destroy();
    return;
  }
  if (refusedByBusy(internals)) {
    socket.end('HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n' +
               'Content-Length: 0\r\n\r\n');
    return;
  }
  const handlers = internals.server.handlers;
  if (kind === 'connect' && handlers.connect === undefined) {
    socket.end('HTTP/1.1 501 Not Implemented\r\nConnection: close\r\n' +
               'Content-Length: 0\r\n\r\n');
    return;
  }
  const res = createLegacyResponse(internals, req, socket);
  let backend;
  try {
    backend = new H1Backend({
      req, res, socket, listener: internals, kind, tunnelStream: stream, head,
    });
  } catch {
    writeSimpleResponse(res, 400);
    return;
  }
  const exchange = createExchange(internals, kind, backend);
  const expect = req.headers.expect;
  exchange.pending100 = typeof expect === 'string' &&
    StringPrototypeToLowerCase(expect) === '100-continue';
  dispatch(handlers, exchange);
}

function onHttp2Stream(internals, stream, headers, flags, rawHeaders) {
  if (internals.destroyed) {
    stream.close(0x8);
    return;
  }
  stream.on('error', noop);
  if (refusedByBusy(internals)) {
    stream.close(0x7);  // REFUSED_STREAM
    return;
  }
  const handlers = internals.server.handlers;
  const method = headers[':method'];
  if (method === 'CONNECT' && handlers.connect === undefined) {
    stream.respond({ ':status': 501 }, { endStream: true });
    return;
  }
  if (method !== 'CONNECT' && headers[':path'] === '*') {
    stream.respond({ ':status': 204 }, { endStream: true });
    return;
  }
  const limit = internals.maxRequestBodySize;
  if (limit > 0 && +(headers['content-length'] ?? 0) > limit) {
    stream.respond({ ':status': 413 }, { endStream: true });
    return;
  }
  let backend;
  try {
    backend = new H2Backend({ stream, headers, flags, rawHeaders,
                              listener: internals });
  } catch {
    stream.respond({ ':status': 400 }, { endStream: true });
    return;
  }
  const exchange = createExchange(internals, backend.kind, backend);
  exchange.pending100 = headers.expect !== undefined &&
    StringPrototypeToLowerCase(`${headers.expect}`) === '100-continue';
  dispatch(handlers, exchange);
}

// =============================================================================
// Binding
// =============================================================================

function createHttp1Server(internals) {
  const http = require('http');
  const { options } = internals;
  const hasConnect = internals.server.handlers.connect !== undefined;
  const server = new http.Server({
    maxHeaderSize: options.limits.maxHeaderSize || undefined,
    requireHostHeader: true,
    shouldUpgradeCallback(req) {
      if (!hasConnect || req.httpVersion === '1.0') return false;
      const connection = req.headers.connection;
      return typeof connection === 'string' &&
        RegExpPrototypeExec(kUpgradeConnection, connection) !== null;
    },
  });
  const timeouts = options.timeouts;
  if (timeouts.headers !== undefined) server.headersTimeout = timeouts.headers;
  if (timeouts.idle !== undefined) server.keepAliveTimeout = timeouts.idle;
  server.requestTimeout = 0;
  if (options.limits.maxRequestsPerConnection !== undefined) {
    server.maxRequestsPerSocket = options.limits.maxRequestsPerConnection;
  }
  server.on('request', (req, res) => onHttp1Request(internals, false, req, res));
  server.on('checkContinue',
            (req, res) => onHttp1Request(internals, true, req, res));
  server.on('checkExpectation', (req, res) => {
    res.writeHead(417);
    res.end();
  });
  server.on('connect', (req, socket, head) =>
    onHttp1Tunnel(internals, 'connect', req, socket, head));
  server.on('upgrade', (req, socket, head) =>
    onHttp1Tunnel(internals, 'upgrade', req, socket, head));
  server.on('clientError', (err, socket) => {
    if (socket.writable && !socket.destroyed) {
      socket.end('HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n');
    }
    socket.destroy();
  });
  // Starts the tracking of headers and request timeouts.
  server.emit('listening');
  return server;
}

function createHttp2Server(internals) {
  const http2 = require('http2');
  const { options } = internals;
  const hasConnect = internals.server.handlers.connect !== undefined;
  const settings = {
    enableConnectProtocol: options.extendedConnect ?? hasConnect,
  };
  if (options.limits.maxConcurrentStreams !== undefined) {
    settings.maxConcurrentStreams = options.limits.maxConcurrentStreams;
  }
  if (options.limits.maxHeaderSize !== undefined) {
    settings.maxHeaderListSize = options.limits.maxHeaderSize;
  }
  const server = http2.createServer({ settings });
  server.on('session', (session) => {
    internals.sessions.add(session);
    session.on('error', noop);
    session.once('close', () => {
      internals.sessions.delete(session);
      maybeFinishClose(internals);
    });
    if (internals.closing) session.close();
    const maxRequests = options.limits.maxRequestsPerConnection;
    if (maxRequests > 0) {
      let count = 0;
      session.on('stream', () => {
        if (++count === maxRequests) session.close();
      });
    }
  });
  server.on('stream', (stream, headers, flags, rawHeaders) =>
    onHttp2Stream(internals, stream, headers, flags, rawHeaders));
  return server;
}

function trackSocket(internals, socket) {
  internals.sockets.add(socket);
  socket.once('close', () => {
    internals.sockets.delete(socket);
    maybeFinishClose(internals);
  });
}

// Detects the HTTP/2 connection preface on a cleartext connection, and hands
// the connection to the HTTP/1.1 or the HTTP/2 server.
function sniffCleartext(internals, socket) {
  let buffered = null;
  const onData = (chunk) => {
    buffered = buffered === null ? chunk :
      require('buffer').Buffer.concat([buffered, chunk]);
    const prefix = buffered.latin1Slice(0, kH2Preface.length);
    const isPreface = prefix === kH2Preface;
    if (!isPreface && StringPrototypeStartsWith(kH2Preface, prefix) &&
        TypedArrayPrototypeGetByteLength(buffered) < kH2Preface.length) {
      return;  // Wait for more data.
    }
    socket.removeListener('data', onData);
    socket.pause();
    if (isPreface) {
      socket.unshift(buffered);
      internals.http2 ??= createHttp2Server(internals);
      internals.http2.emit('connection', socket);
    } else {
      internals.http1 ??= createHttp1Server(internals);
      internals.http1.emit('connection', socket);
      socket.emit('data', buffered);
      socket.resume();
    }
  };
  socket.on('data', onData);
}

function onConnection(internals, socket) {
  trackSocket(internals, socket);
  socket.on('error', noop);
  if (internals.destroyed) {
    socket.destroy();
    return;
  }
  if (internals.options.tls === undefined) {
    sniffCleartext(internals, socket);
    return;
  }
  if (socket.alpnProtocol === 'h2') {
    internals.http2 ??= createHttp2Server(internals);
    internals.http2.emit('connection', socket);
  } else {
    internals.http1 ??= createHttp1Server(internals);
    internals.http1.emit('connection', socket);
  }
}

function toCertificateOptions(certificate, name) {
  if (certificate === null || typeof certificate !== 'object') {
    throw new ERR_INVALID_ARG_TYPE(name, 'Object', certificate);
  }
  const { cert, key } = certificate;
  return { cert, key };
}

function matchSni(map, hostname) {
  const keys = [];
  for (const key in map) ArrayPrototypePush(keys, key);
  const lower = StringPrototypeToLowerCase(hostname);
  for (let i = 0; i < keys.length; i++) {
    if (StringPrototypeToLowerCase(keys[i]) === lower) return map[keys[i]];
  }
  const dot = StringPrototypeIndexOf(lower, '.');
  if (dot > 0) {
    const wildcard = `*${StringPrototypeSlice(lower, dot)}`;
    for (let i = 0; i < keys.length; i++) {
      if (StringPrototypeToLowerCase(keys[i]) === wildcard) return map[keys[i]];
    }
  }
  return null;
}

function createTLSOptions(internals, tlsOptions) {
  const tls = require('tls');
  const alpn = tlsOptions.alpn ?? ['h2', 'http/1.1'];
  for (let i = 0; i < alpn.length; i++) {
    if (alpn[i] !== 'h2' && alpn[i] !== 'http/1.1') {
      throw new ERR_INVALID_ARG_VALUE('tls.alpn', alpn,
                                      'must only contain "h2" and "http/1.1"');
    }
  }
  const result = {
    ALPNProtocols: alpn,
  };
  if (tlsOptions.cert !== undefined) result.cert = tlsOptions.cert;
  if (tlsOptions.key !== undefined) result.key = tlsOptions.key;
  const handshake = internals.options.timeouts.handshake;
  if (handshake !== undefined) result.handshakeTimeout = handshake;
  const sni = tlsOptions.sni;
  if (sni !== undefined) {
    const contexts = { __proto__: null };
    const getContext = (certificate, name) => {
      if (certificate === null || certificate === undefined) return undefined;
      return tls.createSecureContext(toCertificateOptions(certificate, name));
    };
    result.SNICallback = (servername, callback) => {
      if (typeof sni === 'function') {
        PromisePrototypeThen(
          PromiseResolve(FunctionPrototypeCall(sni, undefined,
                                               StringPrototypeToLowerCase(servername))),
          (certificate) => {
            let context;
            try {
              context = getContext(certificate, 'sni()');
            } catch (err) {
              report(err);
              callback(err);
              return;
            }
            callback(null, context);
          },
          (err) => {
            report(err);
            callback(err);
          });
        return;
      }
      const certificate = matchSni(sni, servername);
      if (certificate === null) {
        callback(null, undefined);
        return;
      }
      const key = StringPrototypeToLowerCase(servername);
      try {
        contexts[key] ??= getContext(certificate, 'tls.sni');
      } catch (err) {
        callback(err);
        return;
      }
      callback(null, contexts[key]);
    };
  }
  return result;
}

function bind(internals) {
  const { options } = internals;
  const { promise, resolve, reject } = PromiseWithResolvers();
  let netServer;
  try {
    if (options.tls !== undefined) {
      const tls = require('tls');
      netServer = tls.createServer({
        ...createTLSOptions(internals, options.tls),
        allowHalfOpen: true,
      });
      netServer.on('secureConnection',
                   (socket) => onConnection(internals, socket));
      netServer.on('tlsClientError', noop);
    } else {
      const net = require('net');
      netServer = net.createServer({ allowHalfOpen: true });
      netServer.on('connection', (socket) => onConnection(internals, socket));
    }
  } catch (err) {
    return PromiseReject(err);
  }
  if (options.limits.maxConnections !== undefined) {
    netServer.maxConnections = options.limits.maxConnections;
  }
  internals.netServer = netServer;
  const onError = (err) => {
    netServer.removeListener('listening', onListening);
    reject(err);
  };
  const onListening = () => {
    netServer.removeListener('error', onError);
    netServer.on('error', report);
    resolve();
  };
  netServer.once('error', onError);
  netServer.once('listening', onListening);
  const listenOptions = {
    port: options.port,
    host: options.hostname,
    ipv6Only: options.ipv6Only,
    reusePort: options.reusePort,
  };
  if (options.backlog !== undefined) listenOptions.backlog = options.backlog;
  try {
    netServer.listen(listenOptions);
  } catch (err) {
    netServer.removeListener('error', onError);
    netServer.removeListener('listening', onListening);
    return PromiseReject(err);
  }
  return promise;
}

function validateListenOptions(options) {
  if (options.quic) {
    if (options.tls === undefined) {
      throw new ERR_INVALID_ARG_VALUE('options.quic', options.quic,
                                      'requires options.tls');
    }
    throw notSupported('QUIC is not supported');
  }
  if (options.webTransport === true) {
    throw notSupported('WebTransport is not supported');
  }
  if (options.datagrams === true) {
    throw notSupported('HTTP/3 datagrams are not supported');
  }
}

// =============================================================================
// Server (Section 5.17)
// =============================================================================

let getServerInternals;
let isServer;

class ServerInternals extends Closeable {
  handlers;
  options;
  listeners = [];
  busy = false;
  ready;

  constructor(handlers, options) {
    super();
    this.handlers = handlers;
    this.options = options;
  }

  listenerClosed(listener) {
    const index = ArrayPrototypeIndexOf(this.listeners, listener);
    if (index !== -1) ArrayPrototypeSplice(this.listeners, index, 1);
    this.maybeFinish();
  }

  maybeFinish() {
    if (this.done) return;
    if (!this.closing && !this.destroyed) return;
    if (this.listeners.length > 0) return;
    if (this.destroyed) return;
    this.resolveClosed();
  }
}

class Server {
  #internals;

  static {
    getServerInternals = (server) => server.#internals;
    isServer = (value) => {
      return value != null && typeof value === 'object' && #internals in value;
    };
  }

  constructor(key, internals) {
    if (key !== kConstruct) {
      throw new ERR_INVALID_THIS('Server cannot be constructed');
    }
    this.#internals = internals;
  }

  get ready() {
    return checkServer(this).ready;
  }

  listen(options = undefined) {
    const internals = checkServer(this);
    let resolved;
    try {
      if (internals.closing || internals.destroyed) {
        throw invalidState('The server has been closed');
      }
      if (options !== undefined && options !== null &&
          typeof options !== 'object' && typeof options !== 'function') {
        throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
      }
      resolved = resolveListenOptions(internals.options,
                                      convertListenMembers(options ?? {}));
      validateListenOptions(resolved);
    } catch (err) {
      return PromiseReject(err);
    }
    return listen(internals, resolved);
  }

  get busy() {
    return checkServer(this).busy;
  }

  set busy(value) {
    const internals = checkServer(this);
    internals.busy = !!value;
    for (let i = 0; i < internals.listeners.length; i++) {
      internals.listeners[i].busy = internals.busy;
    }
  }

  close() {
    const internals = checkServer(this);
    closeServer(internals);
    return internals.closedPromise;
  }

  destroy(error = undefined) {
    destroyServer(checkServer(this), error);
  }

  get closed() {
    return checkServer(this).closedPromise;
  }

  [SymbolIterator]() {
    const internals = checkServer(this);
    const listeners = [];
    for (let i = 0; i < internals.listeners.length; i++) {
      ArrayPrototypePush(listeners, internals.listeners[i].listener);
    }
    return new SafeArrayIterator(listeners);
  }

  [SymbolAsyncDispose]() {
    return this.close();
  }

  [kInspect](depth, options) {
    if (depth < 0) return 'Server {}';
    return `Server ${inspect({ listeners: [...this], busy: this.busy },
                             options)}`;
  }
}

function checkServer(self) {
  if (!isServer(self)) throw new ERR_INVALID_THIS('Server');
  return getServerInternals(self);
}

ObjectDefineProperties(Server.prototype, {
  ready: kEnumerableProperty,
  listen: kEnumerableProperty,
  busy: kEnumerableProperty,
  close: kEnumerableProperty,
  destroy: kEnumerableProperty,
  closed: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'Server',
  },
});

function listen(serverInternals, options) {
  const internals = new ListenerInternals(serverInternals, options);
  internals.listener = new Listener(kConstruct, internals);
  ArrayPrototypePush(serverInternals.listeners, internals);
  return PromisePrototypeThen(bind(internals), () => {
    if (serverInternals.destroyed) destroyListener(internals);
    return internals.listener;
  }, (err) => {
    const index = ArrayPrototypeIndexOf(serverInternals.listeners, internals);
    if (index !== -1) ArrayPrototypeSplice(serverInternals.listeners, index, 1);
    throw err;
  });
}

function closeServer(internals) {
  if (!internals.startClose()) return;
  const listeners = [...new SafeArrayIterator(internals.listeners)];
  for (let i = 0; i < listeners.length; i++) {
    closeListener(listeners[i]);
  }
  internals.maybeFinish();
}

function destroyServer(internals, error) {
  if (!internals.startDestroy()) return;
  const listeners = [...new SafeArrayIterator(internals.listeners)];
  for (let i = 0; i < listeners.length; i++) {
    destroyListener(listeners[i], error);
  }
  internals.rejectClosed(error === undefined ? abortError() : error);
}

/**
 * The serve() function (Section 17.1).
 * @param {object} handler
 * @param {object} [options]
 * @returns {Server}
 */
function serve(handler, options = undefined) {
  const handlers = convertHandler(handler);
  const converted = convertServerOptions(options);
  const internals = new ServerInternals(handlers, converted);
  const server = new Server(kConstruct, internals);

  if (converted.signal !== undefined) {
    const signal = converted.signal;
    if (signal.aborted) {
      destroyServer(internals, signal.reason);
    } else {
      signal.addEventListener('abort', () => {
        destroyServer(internals, signal.reason);
      }, { __proto__: null, once: true });
    }
  }

  if (converted.port !== undefined && !internals.destroyed) {
    const resolved = resolveListenOptions(converted, { __proto__: null });
    validateListenOptions(resolved);
    internals.ready = PromisePrototypeThen(listen(internals, resolved),
                                           () => undefined, (err) => {
                                             destroyServer(internals, err);
                                             throw err;
                                           });
  } else {
    internals.ready = PromiseResolve();
  }
  markHandled(internals.ready);
  return server;
}

module.exports = {
  Listener,
  Server,
  serve,
};
