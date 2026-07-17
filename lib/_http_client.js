// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const {
  ArrayIsArray,
  Boolean,
  Error,
  NumberIsFinite,
  ObjectAssign,
  ObjectDefineProperty,
  ObjectKeys,
  ObjectSetPrototypeOf,
  ReflectApply,
  String,
  Symbol,
} = primordials;

const net = require('net');
const assert = require('internal/assert');
const {
  kEmptyObject,
  once,
} = require('internal/util');
const {
  _checkIsHttpToken: checkIsHttpToken,
  freeParser,
  parsers,
  HTTPParser,
  calculateLenientFlags,
  prepareError,
  kSkipPendingData,
} = require('_http_common');
const {
  kUniqueHeaders,
  parseUniqueHeadersOption,
  OutgoingMessage,
} = require('_http_outgoing');
const Agent = require('_http_agent');
const { Buffer } = require('buffer');
const { defaultTriggerAsyncIdScope } = require('internal/async_hooks');
const { URL, urlToHttpOptions, isURL } = require('internal/url');
const {
  kOutHeaders,
  kNeedDrain,
  isTraceHTTPEnabled,
  traceBegin,
  traceEnd,
  getNextTraceEventId,
  kProxyConfig,
  checkShouldUseProxy,
} = require('internal/http');
const {
  ConnResetException,
  codes: {
    ERR_HTTP_HEADERS_SENT,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_HTTP_TOKEN,
    ERR_INVALID_PROTOCOL,
    ERR_UNESCAPED_CHARACTERS,
  },
} = require('internal/errors');
const {
  validateInteger,
  validateBoolean,
  validateOneOf,
  validatePort,
  validateString,
} = require('internal/validators');
const { getTimerDuration } = require('internal/timers');
const {
  hasObserver,
  startPerf,
  stopPerf,
} = require('internal/perf/observe');

const kClientRequestStatistics = Symbol('ClientRequestStatistics');

const dc = require('diagnostics_channel');
const onClientRequestCreatedChannel = dc.channel('http.client.request.created');
const onClientRequestStartChannel = dc.channel('http.client.request.start');
const onClientRequestErrorChannel = dc.channel('http.client.request.error');
const onClientResponseFinishChannel = dc.channel('http.client.response.finish');

function emitErrorEvent(request, error) {
  if (onClientRequestErrorChannel.hasSubscribers) {
    onClientRequestErrorChannel.publish({
      request,
      error,
    });
  }
  request.emit('error', error);
}

const { addAbortSignal, finished } = require('stream');

let debug = require('internal/util/debuglog').debuglog('http', (fn) => {
  debug = fn;
});

const INVALID_PATH_REGEX = /[^\u0021-\u00ff]/;
const kError = Symbol('kError');
const kPath = Symbol('kPath');
const kAuthority = Symbol('kAuthority');
const kProxyRewrittenToAbsolute = Symbol('kProxyRewrittenToAbsolute');

const HTTP_CLIENT_TRACE_EVENT_NAME = 'http.client.request';

function validateHost(host, name) {
  if (host !== null && host !== undefined && typeof host !== 'string') {
    throw new ERR_INVALID_ARG_TYPE(`options.${name}`,
                                   ['string', 'undefined', 'null'],
                                   host);
  }
  return host;
}

class HTTPClientAsyncResource {
  constructor(type, req) {
    this.type = type;
    this.req = req;
  }
}

// The only documented shape is [k, v, k, v, ...]. Here we also accept [[k, v], [k, v], ...].
// for backward compatibility, and reject others. Also reject if there are duplicate Host entries.
// Returns the Host header value, or undefined if absent.
function getHostFromHeaderArray(headers) {
  let host;
  const isPairs = headers.length > 0 && ArrayIsArray(headers[0]);
  if (isPairs) {
    for (let i = 0; i < headers.length; i++) {
      const entry = headers[i];
      if (!ArrayIsArray(entry)) {
        throw new ERR_INVALID_ARG_VALUE(`options.headers[${i}]`, typeof entry,
                                        'must be an array when headers is passed as an array of pairs');
      }
      if (`${entry[0]}`.toLowerCase() === 'host') {
        if (host !== undefined) {
          throw new ERR_INVALID_ARG_VALUE('options.headers', '(redacted)',
                                          'must not contain duplicate Host headers');
        }
        host = `${entry[1]}`;
      }
    }
  } else {
    for (let i = 0; i + 1 < headers.length; i += 2) {
      if (`${headers[i]}`.toLowerCase() === 'host') {
        if (host !== undefined) {
          throw new ERR_INVALID_ARG_VALUE('options.headers', '(redacted)',
                                          'must not contain duplicate Host headers');
        }
        host = `${headers[i + 1]}`;
      }
    }
  }
  return host;
}

function authoritiesMatch(canonicalHost, hostFromHeader) {
  let parsed;
  try {
    parsed = new URL(`http://${hostFromHeader}`);
  } catch {
    return false;
  }
  if (parsed.username || parsed.password ||
      parsed.pathname !== '/' || parsed.search || parsed.hash) {
    return false;
  }
  return parsed.host === canonicalHost;
}

// https://datatracker.ietf.org/doc/html/rfc9112#section-3.2
// When the request target is in absolute-form, ensure it is consistent with
// the request authority: same scheme, no userinfo, and an authority
// component agree with options.host[:port].
function validateRequestAuthority(pathOption, proxyAuthority, userHostHeader, headerArray) {
  validatePort(proxyAuthority.port, 'options.port', true);
  pathOption = `${pathOption}`;
  const requestBase = new URL(`http://${proxyAuthority.host}`);
  requestBase.port = proxyAuthority.port;

  const result = { requestBase };
  if (headerArray !== undefined) {
    const host = getHostFromHeaderArray(headerArray);
    // Since we don't mutate the header array to normalize the Host value, unlike
    // in the case of other shapes of headers provided, we check that it is identical
    // to the authority from the requestBase.
    if (host !== undefined && host !== requestBase.host) {
      throw new ERR_INVALID_ARG_VALUE(
        'Host in options.headers', host,
        `must match the request authority (${requestBase.host})`);
    }
  } else if (userHostHeader !== undefined) {
    if (!authoritiesMatch(requestBase.host, userHostHeader)) {
      throw new ERR_INVALID_ARG_VALUE(
        'Host in options.headers', userHostHeader,
        `must match the request authority (${requestBase.host})`);
    }
  }

  // Per RFC 9112 Section 3.2, if request target is in absolute-form its authority
  // must agree with the request authority.
  let requestURL;
  let isAbsoluteForm = false;
  try {
    requestURL = new URL(pathOption);
    isAbsoluteForm = true;
  } catch {
    if (pathOption.charCodeAt(0) !== 0x2F) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.path', pathOption, 'must be in absolute-form or start with /');
    }
    requestURL = new URL(requestBase.origin + pathOption);
  }
  result.requestURL = requestURL;
  if (!isAbsoluteForm) {
    return result;
  }

  if (requestURL.username || requestURL.password) {
    requestURL.username = '';
    requestURL.password = '';
    throw new ERR_INVALID_ARG_VALUE(
      'options.path', requestURL.href, 'must not contain userinfo, use options.auth instead');
  }

  if (requestURL.protocol !== 'http:') {
    throw new ERR_INVALID_ARG_VALUE(
      'options.path', requestURL.protocol, 'must use http: scheme when specified as an absolute URL');
  }

  if (requestBase.host !== requestURL.host) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.path', requestURL, `must match the request authority (${requestBase.host})`);
  }

  return result;
}

// When proxying a HTTP request, the following needs to be done:
// https://datatracker.ietf.org/doc/html/rfc9112#section-3.2.2
// 1. Rewrite the request path to absolute-form.
// 2. Add proxy-connection and proxy-authorization headers appropriately.
//
// This function checks whether the request should be rewritten for proxying
// and modifies the headers as well as req.path if necessary.
// The handling of the proxy server connection is done in createConnection.
// It also validates that the Host header and absolute-form path authority match the
// request authority specified by reqOptions.
function rewriteForProxiedHttp(req, reqOptions, proxyAuthority, userHostHeader, headerArray) {
  if (req._header) {
    debug('request._header is already sent, skipping rewriteForProxiedHttp', reqOptions);
    return false;
  }
  const agent = req.agent;
  if (!agent || !agent[kProxyConfig]) {
    return false;
  }
  if ((reqOptions.protocol || agent.protocol) !== 'http:') {
    return false;
  }
  // TODO(joyeecheung): cache this on the req or on the agent.
  const shouldUseProxy = checkShouldUseProxy(agent[kProxyConfig], reqOptions);
  debug(`rewriteForProxiedHttp should use proxy for ${reqOptions.host}:${reqOptions.port}:`, shouldUseProxy);
  if (!shouldUseProxy) {
    return false;
  }

  // Per RFC 9112 Section 3.2.2, we don't need to rewrite CONNECT or OPTIONS * requests.
  let requestURL;
  if (req.method !== 'CONNECT' && !(req.method === 'OPTIONS' && req.path === '*')) {
    // Validate Host header values agree with the request authority before mutating req,
    // so a rejected request doesn't leave proxy-* headers stuck on the outgoing header store.
    // XXX(joyeecheung): This validates whether the request conforms to the RFC, but here
    // we only do it for proxied requests for backward compatibility. For non-proxied requests,
    // ensuring thst the request is well formed has been entirely left to the user.
    const result = validateRequestAuthority(req.path, proxyAuthority, userHostHeader, headerArray);
    if (headerArray === undefined) {
      const currentHost = req.getHeader('host');
      if (currentHost !== undefined && currentHost !== result.requestBase.host) {
        req.setHeader('Host', result.requestBase.host);
      }
    }
    requestURL = result.requestURL;
  }

  // Add proxy headers.
  const { auth, href } = agent[kProxyConfig];
  if (auth) {
    req.setHeader('proxy-authorization', auth);
  }
  if (req.shouldKeepAlive) {
    req.setHeader('proxy-connection', 'keep-alive');
  } else {
    req.setHeader('proxy-connection', 'close');
  }

  if (requestURL !== undefined) {
    // Convert the path to absolute-form. The authority is built from options.
    req.path = requestURL.href;
    req[kProxyRewrittenToAbsolute] = true;
  }
  debug(`updated request for HTTP proxy ${href} with ${req.path} `, req[kOutHeaders]);
  return true;
};

function ClientRequest(input, options, cb) {
  OutgoingMessage.call(this);

  if (typeof input === 'string') {
    const urlStr = input;
    input = urlToHttpOptions(new URL(urlStr));
  } else if (isURL(input)) {
    // url.URL instance
    input = urlToHttpOptions(input);
  } else {
    cb = options;
    options = input;
    input = null;
  }

  if (typeof options === 'function') {
    cb = options;
    options = input || kEmptyObject;
  } else {
    options = ObjectAssign({ __proto__: null }, input, options);
  }

  let agent = options.agent;
  const defaultAgent = options._defaultAgent || Agent.globalAgent;
  if (agent === false) {
    agent = new defaultAgent.constructor();
  } else if (agent === null || agent === undefined) {
    if (typeof options.createConnection !== 'function') {
      agent = defaultAgent;
    }
    // Explicitly pass through this statement as agent will not be used
    // when createConnection is provided.
  } else if (typeof agent.addRequest !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('options.agent',
                                   ['Agent-like Object', 'undefined', 'false'],
                                   agent);
  }
  this.agent = agent;

  const protocol = options.protocol || defaultAgent.protocol;
  let expectedProtocol = defaultAgent.protocol;
  if (this.agent?.protocol)
    expectedProtocol = this.agent.protocol;

  if (options.path) {
    const path = String(options.path);
    if (INVALID_PATH_REGEX.test(path)) {
      debug('Path contains unescaped characters: "%s"', path);
      throw new ERR_UNESCAPED_CHARACTERS('Request path');
    }
  }

  if (protocol !== expectedProtocol) {
    throw new ERR_INVALID_PROTOCOL(protocol, expectedProtocol);
  }

  const defaultPort = options.defaultPort ||
                    (this.agent?.defaultPort);

  const optsWithoutSignal = { __proto__: null, ...options };

  const port = optsWithoutSignal.port = options.port || defaultPort || 80;
  const host = optsWithoutSignal.host = validateHost(options.hostname, 'hostname') ||
                                        validateHost(options.host, 'host') || 'localhost';

  const setHost = options.setHost !== undefined ?
    Boolean(options.setHost) :
    options.setDefaultHeaders !== false;

  this._removedConnection = options.setDefaultHeaders === false;
  this._removedContLen = options.setDefaultHeaders === false;
  this._removedTE = options.setDefaultHeaders === false;

  this.socketPath = options.socketPath;

  if (options.timeout !== undefined)
    this.timeout = getTimerDuration(options.timeout, 'timeout');

  const signal = options.signal;
  if (signal) {
    addAbortSignal(signal, this);
    delete optsWithoutSignal.signal;
  }
  let method = options.method;
  if (method != null) {
    validateString(method, 'options.method');
  }

  if (method) {
    if (!checkIsHttpToken(method)) {
      throw new ERR_INVALID_HTTP_TOKEN('Method', method);
    }
    method = this.method = method.toUpperCase();
  } else {
    method = this.method = 'GET';
  }

  const maxHeaderSize = options.maxHeaderSize;
  if (maxHeaderSize !== undefined)
    validateInteger(maxHeaderSize, 'maxHeaderSize', 0);
  this.maxHeaderSize = maxHeaderSize;

  const insecureHTTPParser = options.insecureHTTPParser;
  if (insecureHTTPParser !== undefined) {
    validateBoolean(insecureHTTPParser, 'options.insecureHTTPParser');
  }

  this.insecureHTTPParser = insecureHTTPParser;

  const httpValidation = options.httpValidation;
  if (httpValidation !== undefined) {
    validateOneOf(httpValidation, 'options.httpValidation',
                  ['strict', 'relaxed', 'insecure']);
    if (insecureHTTPParser !== undefined) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.httpValidation',
        httpValidation,
        'cannot be used together with options.insecureHTTPParser',
      );
    }
  }

  this.httpValidation = httpValidation;

  if (options.joinDuplicateHeaders !== undefined) {
    validateBoolean(options.joinDuplicateHeaders, 'options.joinDuplicateHeaders');
  }

  this.joinDuplicateHeaders = options.joinDuplicateHeaders;

  this[kPath] = options.path || '/';
  if (cb) {
    this.once('response', cb);
  }

  if (method === 'GET' ||
      method === 'HEAD' ||
      method === 'DELETE' ||
      method === 'OPTIONS' ||
      method === 'TRACE' ||
      method === 'CONNECT') {
    this.useChunkedEncodingByDefault = false;
  } else {
    this.useChunkedEncodingByDefault = true;
  }

  this._ended = false;
  this.res = null;
  this.aborted = false;
  this.timeoutCb = null;
  this.upgradeOrConnect = false;
  this.parser = null;
  this.maxHeadersCount = null;
  this.reusedSocket = false;
  this.host = host;
  this.protocol = protocol;
  this[kProxyRewrittenToAbsolute] = false;

  if (this.agent) {
    // If there is an agent we should default to Connection:keep-alive,
    // but only if the Agent will actually reuse the connection!
    // If it's not a keepAlive agent, and the maxSockets==Infinity, then
    // there's never a case where this socket will actually be reused
    if (!this.agent.keepAlive && !NumberIsFinite(this.agent.maxSockets)) {
      this._last = true;
      this.shouldKeepAlive = false;
    } else {
      this._last = false;
      this.shouldKeepAlive = true;
    }
  }

  let hostHeaderFromOptions = host;
  // For the Host header, ensure that IPv6 addresses are enclosed
  // in square brackets, as defined by URI formatting
  // https://tools.ietf.org/html/rfc3986#section-3.2.2
  const posColon = hostHeaderFromOptions.indexOf(':');
  if (posColon !== -1 &&
      hostHeaderFromOptions.includes(':', posColon + 1) &&
      hostHeaderFromOptions.charCodeAt(0) !== 91/* '[' */) {
    hostHeaderFromOptions = `[${hostHeaderFromOptions}]`;
  }
  const proxyAuthority = { host: hostHeaderFromOptions, port };

  if (port && +port !== defaultPort) {
    hostHeaderFromOptions += ':' + port;
  }
  // Preserve the request authority (with the port when non-default) so that
  // the perf_hooks entry can report a faithful URL.
  this[kAuthority] = hostHeaderFromOptions;
  const headersArray = ArrayIsArray(options.headers);
  if (!headersArray) {
    if (options.headers) {
      const keys = ObjectKeys(options.headers);
      // Retain for(;;) loop for performance reasons
      // Refs: https://github.com/nodejs/node/pull/30958
      for (let i = 0; i < keys.length; i++) {
        const key = keys[i];
        this.setHeader(key, options.headers[key]);
      }
    }

    // Save the Host header before the implicit auto-set below, so the
    // proxy validator can tell user-explicit values from Node-generated ones.
    const userHostHeader = this.getHeader('host');

    if (host && !this.getHeader('host') && setHost) {
      const hostHeader = method === 'CONNECT' && options.path ?
        String(this.path) : hostHeaderFromOptions;
      this.setHeader('Host', hostHeader);
    }

    if (options.auth && !this.getHeader('Authorization')) {
      this.setHeader('Authorization', 'Basic ' +
                     Buffer.from(options.auth).toString('base64'));
    }

    if (this.getHeader('expect')) {
      if (this._header) {
        throw new ERR_HTTP_HEADERS_SENT('render');
      }

      rewriteForProxiedHttp(this, optsWithoutSignal, proxyAuthority, userHostHeader);
      this._storeHeader(this.method + ' ' + this.path + ' HTTP/1.1\r\n',
                        this[kOutHeaders]);
    } else {
      rewriteForProxiedHttp(this, optsWithoutSignal, proxyAuthority, userHostHeader);
    }
  } else {
    rewriteForProxiedHttp(this, optsWithoutSignal, proxyAuthority, undefined, options.headers);
    this._storeHeader(this.method + ' ' + this.path + ' HTTP/1.1\r\n',
                      options.headers);
  }

  this[kUniqueHeaders] = parseUniqueHeadersOption(options.uniqueHeaders);

  // initiate connection
  if (this.agent) {
    this.agent.addRequest(this, optsWithoutSignal);
  } else {
    // No agent, default to Connection:close.
    this._last = true;
    this.shouldKeepAlive = false;
    let opts = optsWithoutSignal;
    if (opts.path || opts.socketPath) {
      opts = { ...optsWithoutSignal };
      if (opts.socketPath) {
        opts.path = opts.socketPath;
      } else {
        opts.path &&= undefined;
      }
    }
    if (typeof opts.createConnection === 'function') {
      const oncreate = once((err, socket) => {
        if (err) {
          process.nextTick(() => emitErrorEvent(this, err));
        } else {
          this.onSocket(socket);
        }
      });

      try {
        const newSocket = opts.createConnection(opts, oncreate);
        if (newSocket) {
          oncreate(null, newSocket);
        }
      } catch (err) {
        oncreate(err);
      }
    } else {
      debug('CLIENT use net.createConnection', opts);
      this.onSocket(net.createConnection(opts));
    }
  }
  if (onClientRequestCreatedChannel.hasSubscribers) {
    onClientRequestCreatedChannel.publish({
      request: this,
    });
  }
}
ObjectSetPrototypeOf(ClientRequest.prototype, OutgoingMessage.prototype);
ObjectSetPrototypeOf(ClientRequest, OutgoingMessage);

ObjectDefineProperty(ClientRequest.prototype, 'path', {
  __proto__: null,
  get() {
    return this[kPath];
  },
  set(value) {
    const path = String(value);
    if (INVALID_PATH_REGEX.test(path)) {
      throw new ERR_UNESCAPED_CHARACTERS('Request path');
    }
    this[kPath] = path;
  },
  configurable: true,
  enumerable: true,
});

ClientRequest.prototype._finish = function _finish() {
  OutgoingMessage.prototype._finish.call(this);
  if (hasObserver('http')) {
    startPerf(this, kClientRequestStatistics, {
      type: 'http',
      name: 'HttpClient',
      detail: {
        req: {
          method: this.method,
          // If the path has been rewritten to absolute-form for proxying,
          // it is already a full URL.
          url: this[kProxyRewrittenToAbsolute] ?
            this.path :
            `${this.protocol}//${this[kAuthority]}${this.path}`,
          headers: typeof this.getHeaders === 'function' ? this.getHeaders() : {},
        },
      },
    });
  }
  if (onClientRequestStartChannel.hasSubscribers) {
    onClientRequestStartChannel.publish({
      request: this,
    });
  }
  if (isTraceHTTPEnabled()) {
    this._traceEventId = getNextTraceEventId();
    traceBegin(HTTP_CLIENT_TRACE_EVENT_NAME, this._traceEventId);
  }
};

ClientRequest.prototype._implicitHeader = function _implicitHeader() {
  if (this._header) {
    throw new ERR_HTTP_HEADERS_SENT('render');
  }
  this._storeHeader(this.method + ' ' + this.path + ' HTTP/1.1\r\n',
                    this[kOutHeaders]);
};

ClientRequest.prototype.abort = function abort() {
  if (this.aborted) {
    return;
  }
  this.aborted = true;
  process.nextTick(emitAbortNT, this);
  this.destroy();
};

ClientRequest.prototype.destroy = function destroy(err) {
  if (this.destroyed) {
    return this;
  }
  this.destroyed = true;

  // If we're aborting, we don't care about any more response data.
  if (this.res) {
    this.res._dump();
  }

  this[kError] = err;
  this.socket?.destroy(err);

  return this;
};

function emitAbortNT(req) {
  req.emit('abort');
}

function ondrain() {
  const msg = this._httpMessage;
  if (msg && !msg.finished && msg[kNeedDrain]) {
    msg[kNeedDrain] = false;
    msg.emit('drain');
  }
}

function socketCloseListener() {
  const socket = this;
  const req = socket._httpMessage;
  debug('HTTP socket close');

  // NOTE: It's important to get parser here, because it could be freed by
  // the `socketOnData`.
  const parser = socket.parser;
  const res = req.res;

  req.destroyed = true;
  if (res) {
    // Socket closed before we emitted 'end' below.
    if (!res.complete) {
      res.destroy(new ConnResetException('aborted'));
    }
    req._closed = true;
    req.emit('close');
    if (!res.aborted && res.readable) {
      res.push(null);
    }
  } else {
    if (!req.socket._hadError) {
      // This socket error fired before we started to
      // receive a response. The error needs to
      // fire on the request.
      req.socket._hadError = true;
      emitErrorEvent(req, new ConnResetException('socket hang up'));
    }
    req._closed = true;
    req.emit('close');
  }

  // Too bad.  That output wasn't getting written.
  // This is pretty terrible that it doesn't raise an error.
  // Fixed better in v0.10
  if (req.outputData)
    req.outputData.length = 0;

  if (parser) {
    parser.finish();
    freeParser(parser, req, socket);
  }
}

function socketErrorListener(err) {
  const socket = this;
  const req = socket._httpMessage;
  debug('SOCKET ERROR:', err.message, err.stack);

  if (req) {
    const res = req.res;
    const isUserDestroyError = err === req[kError] || err === res?.errored;

    // For Safety. Some additional errors might fire later on
    // and we need to make sure we don't double-fire the error event.
    socket._hadError = true;
    // Before a response exists, the request itself failed. Once a response
    // exists, socket teardown belongs to the IncomingMessage and is finalized
    // by socketCloseListener. Preserve errors explicitly used to destroy the
    // request or response, which have historically been emitted on the request.
    if (!res || isUserDestroyError) {
      emitErrorEvent(req, err);
    }

    if (res) {
      socket.destroy();
      return;
    }
  }

  const parser = socket.parser;
  if (parser) {
    parser.finish();
    freeParser(parser, req, socket);
  }

  // Ensure that no further data will come out of the socket
  socket.removeListener('data', socketOnData);
  socket.removeListener('end', socketOnEnd);
  socket.destroy();
}

function socketOnEnd() {
  const socket = this;
  const req = this._httpMessage;
  const parser = this.parser;

  if (!req.res && !req.socket._hadError) {
    // If we don't have a response then we know that the socket
    // ended prematurely and we need to emit an error on the request.
    req.socket._hadError = true;
    emitErrorEvent(req, new ConnResetException('socket hang up'));
  }
  if (parser) {
    parser.finish();
    freeParser(parser, req, socket);
  }
  socket.destroy();
}

function socketOnData(d) {
  const socket = this;
  const req = this._httpMessage;
  const parser = this.parser;

  assert(parser && parser.socket === socket);

  const ret = parser.execute(d);
  if (ret instanceof Error) {
    prepareError(ret, parser, d);
    debug('parse error', ret);
    freeParser(parser, req, socket);
    socket.removeListener('data', socketOnData);
    socket.removeListener('end', socketOnEnd);
    socket.destroy();
    req.socket._hadError = true;
    emitErrorEvent(req, ret);
  } else if (parser.incoming?.upgrade) {
    // Upgrade (if status code 101) or CONNECT
    const bytesParsed = ret;
    const res = parser.incoming;
    req.res = res;

    socket.removeListener('data', socketOnData);
    socket.removeListener('end', socketOnEnd);
    socket.removeListener('drain', ondrain);

    if (req.timeoutCb) socket.removeListener('timeout', req.timeoutCb);
    socket.removeListener('timeout', responseOnTimeout);

    parser.finish();
    freeParser(parser, req, socket);

    const bodyHead = d.slice(bytesParsed, d.length);

    const eventName = req.method === 'CONNECT' ? 'connect' : 'upgrade';
    if (req.listenerCount(eventName) > 0) {
      req.upgradeOrConnect = true;

      // detach the socket
      socket.emit('agentRemove');
      socket.removeListener('close', socketCloseListener);
      socket.removeListener('error', socketErrorListener);

      socket._httpMessage = null;
      socket.readableFlowing = null;

      req.emit(eventName, res, socket, bodyHead);
      req.destroyed = true;
      req._closed = true;
      req.emit('close');
    } else {
      // Requested Upgrade or used CONNECT method, but have no handler.
      socket.destroy();
    }
  } else if (parser.incoming?.complete &&
             // When the status code is informational (100, 102-199),
             // the server will send a final response after this client
             // sends a request body, so we must not free the parser.
             // 101 (Switching Protocols) and all other status codes
             // should be processed normally.
             !statusIsInformational(parser.incoming.statusCode)) {
    socket.removeListener('data', socketOnData);
    socket.removeListener('end', socketOnEnd);
    socket.removeListener('drain', ondrain);
    freeParser(parser, req, socket);
  }
}

function statusIsInformational(status) {
  // 100 (Continue)    RFC7231 Section 6.2.1
  // 102 (Processing)  RFC2518
  // 103 (Early Hints) RFC8297
  // 104-199 (Unassigned)
  return (status < 200 && status >= 100 && status !== 101);
}

// client
function parserOnIncomingClient(res, shouldKeepAlive) {
  const socket = this.socket;
  const req = socket._httpMessage;

  debug('AGENT incoming response!');

  if (req.res) {
    // We already have a response object, this means the server
    // sent a double response.
    socket.destroy();
    if (socket.parser) {
      // https://github.com/nodejs/node/issues/60025
      // Now, parser.incoming is pointed to the new IncomingMessage,
      // we need to rewrite it to the first one and skip all the pending IncomingMessage
      socket.parser.incoming = req.res;
      socket.parser.incoming[kSkipPendingData] = true;
    }
    return 0;
  }
  req.res = res;

  // Skip body and treat as Upgrade.
  if (res.upgrade)
    return 2;

  // Responses to CONNECT request is handled as Upgrade.
  const method = req.method;
  if (method === 'CONNECT') {
    res.upgrade = true;
    return 2;  // Skip body and treat as Upgrade.
  }

  if (statusIsInformational(res.statusCode)) {
    // Restart the parser, as this is a 1xx informational message.
    req.res = null; // Clear res so that we don't hit double-responses.
    // Maintain compatibility by sending 100-specific events
    if (res.statusCode === 100) {
      req.emit('continue');
    }
    // Send information events to all 1xx responses except 101 Upgrade.
    req.emit('information', {
      statusCode: res.statusCode,
      statusMessage: res.statusMessage,
      httpVersion: res.httpVersion,
      httpVersionMajor: res.httpVersionMajor,
      httpVersionMinor: res.httpVersionMinor,
      headers: res.headers,
      rawHeaders: res.rawHeaders,
    });

    return 1;  // Skip body but don't treat as Upgrade.
  }

  if (req.shouldKeepAlive && !shouldKeepAlive && !req.upgradeOrConnect) {
    // Server MUST respond with Connection:keep-alive for us to enable it.
    // If we've been upgraded (via WebSockets) we also shouldn't try to
    // keep the connection open.
    req.shouldKeepAlive = false;
  }

  if (req[kClientRequestStatistics] && hasObserver('http')) {
    stopPerf(req, kClientRequestStatistics, {
      detail: {
        res: {
          statusCode: res.statusCode,
          statusMessage: res.statusMessage,
          headers: res.headers,
        },
      },
    });
  }
  if (onClientResponseFinishChannel.hasSubscribers) {
    onClientResponseFinishChannel.publish({
      request: req,
      response: res,
    });
  }
  if (isTraceHTTPEnabled() && typeof req._traceEventId === 'number') {
    traceEnd(HTTP_CLIENT_TRACE_EVENT_NAME, req._traceEventId, {
      path: req.path,
      statusCode: res.statusCode,
    });
  }
  req.res = res;
  res.req = req;

  // Add our listener first, so that we guarantee socket cleanup
  res.on('end', responseOnEnd);
  req.on('finish', requestOnFinish);
  socket.on('timeout', responseOnTimeout);

  // If the user did not listen for the 'response' event, then they
  // can't possibly read the data, so we ._dump() it into the void
  // so that the socket doesn't hang there in a paused state.
  if (req.aborted || !req.emit('response', res))
    res._dump();

  if (method === 'HEAD')
    return 1;  // Skip body but don't treat as Upgrade.

  if (res.statusCode === 304) {
    res.complete = true;
    return 1; // Skip body as there won't be any
  }

  return 0;  // No special treatment.
}

// client
function responseKeepAlive(req) {
  const socket = req.socket;

  debug('AGENT socket keep-alive');
  if (req.timeoutCb) {
    socket.setTimeout(0, req.timeoutCb);
    req.timeoutCb = null;
  }
  socket.removeListener('close', socketCloseListener);
  socket.removeListener('error', socketErrorListener);
  socket.removeListener('data', socketOnData);
  socket.removeListener('end', socketOnEnd);

  // TODO(ronag): Between here and emitFreeNT the socket
  // has no 'error' handler.

  // There are cases where _handle === null. Avoid those. Passing undefined to
  // nextTick() will call getDefaultTriggerAsyncId() to retrieve the id.
  const asyncId = socket._handle ? socket._handle.getAsyncId() : undefined;
  // Mark this socket as available, AFTER user-added end
  // handlers have a chance to run.
  defaultTriggerAsyncIdScope(asyncId, process.nextTick, emitFreeNT, req);

  req.destroyed = true;
  if (req.res) {
    // Detach socket from IncomingMessage to avoid destroying the freed
    // socket in IncomingMessage.destroy().
    req.res.socket = null;
  }
}

function responseOnEnd() {
  const req = this.req;
  const socket = req.socket;

  if (socket) {
    if (req.timeoutCb) socket.removeListener('timeout', emitRequestTimeout);
    socket.removeListener('timeout', responseOnTimeout);
  }

  req._ended = true;

  if (!req.shouldKeepAlive) {
    if (socket.writable) {
      debug('AGENT socket.destroySoon()');
      if (typeof socket.destroySoon === 'function')
        socket.destroySoon();
      else
        socket.end();
    }
    assert(!socket.writable);
  } else if (req.writableFinished && !this.aborted) {
    assert(req.finished);
    // We can assume `req.finished` means all data has been written since:
    // - `'responseOnEnd'` means we have been assigned a socket.
    // - when we have a socket we write directly to it without buffering.
    // - `req.finished` means `end()` has been called and no further data.
    //   can be written
    // In addition, `req.writableFinished` means all data written has been
    // accepted by the kernel. (i.e. the `req.socket` is drained).Without
    // this constraint, we may assign a non drained socket to a request.
    responseKeepAlive(req);
  }
}

function responseOnTimeout() {
  const req = this._httpMessage;
  if (!req) return;
  const res = req.res;
  if (!res) return;
  res.emit('timeout');
}

// This function is necessary in the case where we receive the entire response
// from the server before we finish sending out the request.
function requestOnFinish() {
  const req = this;

  // If the response ends before this request finishes writing, `responseOnEnd()`
  // already released the socket. When `finish` fires later, that socket may
  // belong to a different request, so only call `responseKeepAlive()` when the
  // original request is still alive (`!req.destroyed`).
  if (req.shouldKeepAlive && req._ended && !req.destroyed)
    responseKeepAlive(req);
}

function emitFreeNT(req) {
  req._closed = true;
  req.emit('close');
  if (req.socket) {
    req.socket.emit('free');
  }
}

function tickOnSocket(req, socket) {
  const parser = parsers.alloc();
  req.socket = socket;
  const lenientFlags = calculateLenientFlags(req.httpValidation, req.insecureHTTPParser);
  parser.initialize(HTTPParser.RESPONSE,
                    new HTTPClientAsyncResource('HTTPINCOMINGMESSAGE', req),
                    req.maxHeaderSize || 0,
                    lenientFlags);
  parser.socket = socket;
  parser.outgoing = req;
  req.parser = parser;

  socket.parser = parser;
  socket._httpMessage = req;

  // Propagate headers limit from request object to parser
  if (typeof req.maxHeadersCount === 'number') {
    parser.maxHeaderPairs = req.maxHeadersCount << 1;
  }

  parser.joinDuplicateHeaders = req.joinDuplicateHeaders;

  parser.onIncoming = parserOnIncomingClient;
  socket.on('data', socketOnData);
  socket.on('end', socketOnEnd);
  socket.on('close', socketCloseListener);
  socket.on('drain', ondrain);

  if (
    req.timeout !== undefined ||
    (req.agent?.options?.timeout)
  ) {
    listenSocketTimeout(req);
  }
  req.emit('socket', socket);
}

function emitRequestTimeout() {
  const req = this._httpMessage;
  if (req) {
    req.emit('timeout');
  }
}

function listenSocketTimeout(req) {
  if (req.timeoutCb) {
    return;
  }
  // Set timeoutCb so it will get cleaned up on request end.
  req.timeoutCb = emitRequestTimeout;
  // Delegate socket timeout event.
  if (req.socket) {
    req.socket.once('timeout', emitRequestTimeout);
  } else {
    req.on('socket', (socket) => {
      socket.once('timeout', emitRequestTimeout);
    });
  }
}

ClientRequest.prototype.onSocket = function onSocket(socket, err) {
  // Attach the error listener synchronously so that any errors emitted on
  // the socket before onSocketNT runs (e.g. from a blocklist check or other
  // next-tick error) are forwarded to the request and can be caught by the
  // user's error handler. socketErrorListener requires socket._httpMessage
  // to be set so we set it here too.
  if (socket && !err) {
    socket._httpMessage = this;
    socket.on('error', socketErrorListener);
  }
  process.nextTick(onSocketNT, this, socket, err);
};

function onSocketNT(req, socket, err) {
  if (req.destroyed || err) {
    req.destroyed = true;

    function _destroy(req, err) {
      if (!req.aborted && !err) {
        err = new ConnResetException('socket hang up');
      }
      // ERR_PROXY_TUNNEL is handled by the proxying logic.
      // Skip if the error was already emitted by the early socketErrorListener.
      if (err && err.code !== 'ERR_PROXY_TUNNEL' &&
          !socket?._hadError) {
        emitErrorEvent(req, err);
      }
      req._closed = true;
      req.emit('close');
    }

    if (socket) {
      if (!err && req.agent && !socket.destroyed) {
        socket.emit('free');
        socket.removeListener('error', socketErrorListener);
      } else {
        finished(socket.destroy(err || req[kError]), (er) => {
          if (er?.code === 'ERR_STREAM_PREMATURE_CLOSE') {
            er = null;
          }
          _destroy(req, er || err);
        });
        return;
      }
    }

    _destroy(req, err || req[kError]);
  } else {
    tickOnSocket(req, socket);
    req._flush();
  }
}

ClientRequest.prototype._deferToConnect = _deferToConnect;
function _deferToConnect(method, arguments_) {
  // This function is for calls that need to happen once the socket is
  // assigned to this request and writable. It's an important promisy
  // thing for all the socket calls that happen either now
  // (when a socket is assigned) or in the future (when a socket gets
  // assigned out of the pool and is eventually writable).

  const callSocketMethod = () => {
    if (method)
      ReflectApply(this.socket[method], this.socket, arguments_);
  };

  const onSocket = () => {
    if (this.socket.writable) {
      callSocketMethod();
    } else {
      this.socket.once('connect', callSocketMethod);
    }
  };

  if (!this.socket) {
    this.once('socket', onSocket);
  } else {
    onSocket();
  }
}

ClientRequest.prototype.setTimeout = function setTimeout(msecs, callback) {
  if (this._ended) {
    return this;
  }

  listenSocketTimeout(this);
  msecs = getTimerDuration(msecs, 'msecs');
  if (callback) this.once('timeout', callback);

  if (this.socket) {
    setSocketTimeout(this.socket, msecs);
  } else {
    this.once('socket', (sock) => setSocketTimeout(sock, msecs));
  }

  return this;
};

function setSocketTimeout(sock, msecs) {
  if (sock.connecting) {
    sock.once('connect', function() {
      sock.setTimeout(msecs);
    });
  } else {
    sock.setTimeout(msecs);
  }
}

ClientRequest.prototype.setNoDelay = function setNoDelay(noDelay) {
  this._deferToConnect('setNoDelay', [noDelay]);
};

ClientRequest.prototype.setSocketKeepAlive =
    function setSocketKeepAlive(enable, initialDelay) {
      this._deferToConnect('setKeepAlive', [enable, initialDelay]);
    };

ClientRequest.prototype.clearTimeout = function clearTimeout(cb) {
  this.setTimeout(0, cb);
};

module.exports = {
  ClientRequest,
};
