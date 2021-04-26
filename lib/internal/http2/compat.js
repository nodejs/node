'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  Boolean,
  FunctionPrototypeBind,
  ObjectAssign,
  ObjectCreate,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  Proxy,
  ReflectApply,
  ReflectGetPrototypeOf,
  StringPrototypeIncludes,
  SafeArrayIterator,
  StringPrototypeToLowerCase,
  StringPrototypeTrim,
  Symbol,
} = primordials;

const assert = require('internal/assert');
const Stream = require('stream');
const { Readable } = Stream;
const {
  constants: {
    HTTP2_HEADER_AUTHORITY,
    HTTP2_HEADER_CONNECTION,
    HTTP2_HEADER_METHOD,
    HTTP2_HEADER_PATH,
    HTTP2_HEADER_SCHEME,
    HTTP2_HEADER_STATUS,

    HTTP_STATUS_CONTINUE,
    HTTP_STATUS_EXPECTATION_FAILED,
    HTTP_STATUS_METHOD_NOT_ALLOWED,
    HTTP_STATUS_OK
  }
} = internalBinding('http2');
const {
  codes: {
    ERR_HTTP2_HEADERS_SENT,
    ERR_HTTP2_INFO_STATUS_NOT_ALLOWED,
    ERR_HTTP2_INVALID_HEADER_VALUE,
    ERR_HTTP2_INVALID_STREAM,
    ERR_HTTP2_NO_SOCKET_MANIPULATION,
    ERR_HTTP2_PSEUDOHEADER_NOT_ALLOWED,
    ERR_HTTP2_STATUS_INVALID,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_HTTP_TOKEN,
    ERR_STREAM_WRITE_AFTER_END
  },
  hideStackFrames
} = require('internal/errors');
const {
  validateCallback,
  validateString,
} = require('internal/validators');
const {
  kSocket,
  kRequest,
  kProxySocket,
  assertValidPseudoHeader,
  getAuthority
} = require('internal/http2/util');
const { _checkIsHttpToken: checkIsHttpToken } = require('_http_common');

const kBeginSend = Symbol('begin-send');
const kState = Symbol('state');
const kStream = Symbol('stream');
const kResponse = Symbol('response');
const kHeaders = Symbol('headers');
const kRawHeaders = Symbol('rawHeaders');
const kTrailers = Symbol('trailers');
const kRawTrailers = Symbol('rawTrailers');
const kSetHeader = Symbol('setHeader');
const kAborted = Symbol('aborted');

let statusMessageWarned = false;
let statusConnectionHeaderWarned = false;

// Defines and implements an API compatibility layer on top of the core
// HTTP/2 implementation, intended to provide an interface that is as
// close as possible to the current require('http') API

const assertValidHeader = hideStackFrames((name, value) => {
  if (name === '' ||
      typeof name !== 'string' ||
      StringPrototypeIncludes(name, ' ')) {
    throw new ERR_INVALID_HTTP_TOKEN('Header name', name);
  }
  if (isPseudoHeader(name)) {
    throw new ERR_HTTP2_PSEUDOHEADER_NOT_ALLOWED();
  }
  if (value === undefined || value === null) {
    throw new ERR_HTTP2_INVALID_HEADER_VALUE(value, name);
  }
  if (!isConnectionHeaderAllowed(name, value)) {
    connectionHeaderMessageWarn();
  }
});

function isPseudoHeader(name) {
  switch (name) {
    case HTTP2_HEADER_STATUS:    // :status
    case HTTP2_HEADER_METHOD:    // :method
    case HTTP2_HEADER_PATH:      // :path
    case HTTP2_HEADER_AUTHORITY: // :authority
    case HTTP2_HEADER_SCHEME:    // :scheme
      return true;
    default:
      return false;
  }
}

function statusMessageWarn() {
  if (statusMessageWarned === false) {
    process.emitWarning(
      'Status message is not supported by HTTP/2 (RFC7540 8.1.2.4)',
      'UnsupportedWarning'
    );
    statusMessageWarned = true;
  }
}

function isConnectionHeaderAllowed(name, value) {
  return name !== HTTP2_HEADER_CONNECTION ||
         value === 'trailers';
}

function connectionHeaderMessageWarn() {
  if (statusConnectionHeaderWarned === false) {
    process.emitWarning(
      'The provided connection header is not valid, ' +
      'the value will be dropped from the header and ' +
      'will never be in use.',
      'UnsupportedWarning'
    );
    statusConnectionHeaderWarned = true;
  }
}

function onStreamData(chunk) {
  const request = this[kRequest];
  if (request !== undefined && !request.push(chunk))
    this.pause();
}

function onStreamTrailers(trailers, flags, rawTrailers) {
  const request = this[kRequest];
  if (request !== undefined) {
    ObjectAssign(request[kTrailers], trailers);
    ArrayPrototypePush(request[kRawTrailers],
                       ...new SafeArrayIterator(rawTrailers));
  }
}

function onStreamEnd() {
  // Cause the request stream to end as well.
  const request = this[kRequest];
  if (request !== undefined)
    this[kRequest].push(null);
}

function onStreamError(error) {
  // This is purposefully left blank
  //
  // errors in compatibility mode are
  // not forwarded to the request
  // and response objects.
}

function onRequestPause() {
  this[kStream].pause();
}

function onRequestResume() {
  this[kStream].resume();
}

function onStreamDrain() {
  const response = this[kResponse];
  if (response !== undefined)
    response.emit('drain');
}

function onStreamAbortedRequest() {
  const request = this[kRequest];
  if (request !== undefined && request[kState].closed === false) {
    request[kAborted] = true;
    request.emit('aborted');
  }
}

function onStreamAbortedResponse() {
  // non-op for now
}

function resumeStream(stream) {
  stream.resume();
}

const proxySocketHandler = {
  has(stream, prop) {
    const ref = stream.session !== undefined ? stream.session[kSocket] : stream;
    return (prop in stream) || (prop in ref);
  },

  get(stream, prop) {
    switch (prop) {
      case 'on':
      case 'once':
      case 'end':
      case 'emit':
      case 'destroy':
        return FunctionPrototypeBind(stream[prop], stream);
      case 'writable':
      case 'destroyed':
        return stream[prop];
      case 'readable':
        if (stream.destroyed)
          return false;
        const request = stream[kRequest];
        return request ? request.readable : stream.readable;
      case 'setTimeout':
        const session = stream.session;
        if (session !== undefined)
          return FunctionPrototypeBind(session.setTimeout, session);
        return FunctionPrototypeBind(stream.setTimeout, stream);
      case 'write':
      case 'read':
      case 'pause':
      case 'resume':
        throw new ERR_HTTP2_NO_SOCKET_MANIPULATION();
      default:
        const ref = stream.session !== undefined ?
          stream.session[kSocket] : stream;
        const value = ref[prop];
        return typeof value === 'function' ?
          FunctionPrototypeBind(value, ref) :
          value;
    }
  },
  getPrototypeOf(stream) {
    if (stream.session !== undefined)
      return ReflectGetPrototypeOf(stream.session[kSocket]);
    return ReflectGetPrototypeOf(stream);
  },
  set(stream, prop, value) {
    switch (prop) {
      case 'writable':
      case 'readable':
      case 'destroyed':
      case 'on':
      case 'once':
      case 'end':
      case 'emit':
      case 'destroy':
        stream[prop] = value;
        return true;
      case 'setTimeout':
        const session = stream.session;
        if (session !== undefined)
          session.setTimeout = value;
        else
          stream.setTimeout = value;
        return true;
      case 'write':
      case 'read':
      case 'pause':
      case 'resume':
        throw new ERR_HTTP2_NO_SOCKET_MANIPULATION();
      default:
        const ref = stream.session !== undefined ?
          stream.session[kSocket] : stream;
        ref[prop] = value;
        return true;
    }
  }
};

function onStreamCloseRequest() {
  const req = this[kRequest];

  if (req === undefined)
    return;

  const state = req[kState];
  state.closed = true;

  req.push(null);
  // If the user didn't interact with incoming data and didn't pipe it,
  // dump it for compatibility with http1
  if (!state.didRead && !req._readableState.resumeScheduled)
    req.resume();

  this[kProxySocket] = null;
  this[kRequest] = undefined;

  req.emit('close');
}

function onStreamTimeout(kind) {
  return function onStreamTimeout() {
    const obj = this[kind];
    obj.emit('timeout');
  };
}

class Http2ServerRequest extends Readable {
  constructor(stream, headers, options, rawHeaders) {
    super({ autoDestroy: false, ...options });
    this[kState] = {
      closed: false,
      didRead: false,
    };
    // Headers in HTTP/1 are not initialized using Object.create(null) which,
    // although preferable, would simply break too much code. Ergo header
    // initialization using Object.create(null) in HTTP/2 is intentional.
    this[kHeaders] = headers;
    this[kRawHeaders] = rawHeaders;
    this[kTrailers] = {};
    this[kRawTrailers] = [];
    this[kStream] = stream;
    this[kAborted] = false;
    stream[kProxySocket] = null;
    stream[kRequest] = this;

    // Pause the stream..
    stream.on('trailers', onStreamTrailers);
    stream.on('end', onStreamEnd);
    stream.on('error', onStreamError);
    stream.on('aborted', onStreamAbortedRequest);
    stream.on('close', onStreamCloseRequest);
    stream.on('timeout', onStreamTimeout(kRequest));
    this.on('pause', onRequestPause);
    this.on('resume', onRequestResume);
  }

  get aborted() {
    return this[kAborted];
  }

  get complete() {
    return this[kAborted] ||
           this.readableEnded ||
           this[kState].closed ||
           this[kStream].destroyed;
  }

  get stream() {
    return this[kStream];
  }

  get headers() {
    return this[kHeaders];
  }

  get rawHeaders() {
    return this[kRawHeaders];
  }

  get trailers() {
    return this[kTrailers];
  }

  get rawTrailers() {
    return this[kRawTrailers];
  }

  get httpVersionMajor() {
    return 2;
  }

  get httpVersionMinor() {
    return 0;
  }

  get httpVersion() {
    return '2.0';
  }

  get socket() {
    const stream = this[kStream];
    const proxySocket = stream[kProxySocket];
    if (proxySocket === null)
      return stream[kProxySocket] = new Proxy(stream, proxySocketHandler);
    return proxySocket;
  }

  get connection() {
    return this.socket;
  }

  _read(nread) {
    const state = this[kState];
    assert(!state.closed);
    if (!state.didRead) {
      state.didRead = true;
      this[kStream].on('data', onStreamData);
    } else {
      process.nextTick(resumeStream, this[kStream]);
    }
  }

  get method() {
    return this[kHeaders][HTTP2_HEADER_METHOD];
  }

  set method(method) {
    validateString(method, 'method');
    if (StringPrototypeTrim(method) === '')
      throw new ERR_INVALID_ARG_VALUE('method', method);

    this[kHeaders][HTTP2_HEADER_METHOD] = method;
  }

  get authority() {
    return getAuthority(this[kHeaders]);
  }

  get scheme() {
    return this[kHeaders][HTTP2_HEADER_SCHEME];
  }

  get url() {
    return this[kHeaders][HTTP2_HEADER_PATH];
  }

  set url(url) {
    this[kHeaders][HTTP2_HEADER_PATH] = url;
  }

  setTimeout(msecs, callback) {
    if (!this[kState].closed)
      this[kStream].setTimeout(msecs, callback);
    return this;
  }
}

function onStreamTrailersReady() {
  this.sendTrailers(this[kResponse][kTrailers]);
}

function onStreamCloseResponse() {
  const res = this[kResponse];

  if (res === undefined)
    return;

  const state = res[kState];

  if (this.headRequest !== state.headRequest)
    return;

  state.closed = true;

  this[kProxySocket] = null;

  this.removeListener('wantTrailers', onStreamTrailersReady);
  this[kResponse] = undefined;

  res.emit('finish');
  res.emit('close');
}

class Http2ServerResponse extends Stream {
  constructor(stream, options) {
    super(options);
    this[kState] = {
      closed: false,
      ending: false,
      destroyed: false,
      headRequest: false,
      sendDate: true,
      statusCode: HTTP_STATUS_OK,
    };
    this[kHeaders] = ObjectCreate(null);
    this[kTrailers] = ObjectCreate(null);
    this[kStream] = stream;
    stream[kProxySocket] = null;
    stream[kResponse] = this;
    this.writable = true;
    this.req = stream[kRequest];
    stream.on('drain', onStreamDrain);
    stream.on('aborted', onStreamAbortedResponse);
    stream.on('close', onStreamCloseResponse);
    stream.on('wantTrailers', onStreamTrailersReady);
    stream.on('timeout', onStreamTimeout(kResponse));
  }

  // User land modules such as finalhandler just check truthiness of this
  // but if someone is actually trying to use this for more than that
  // then we simply can't support such use cases
  get _header() {
    return this.headersSent;
  }

  get writableEnded() {
    const state = this[kState];
    return state.ending;
  }

  get finished() {
    const state = this[kState];
    return state.ending;
  }

  get socket() {
    // This is compatible with http1 which removes socket reference
    // only from ServerResponse but not IncomingMessage
    if (this[kState].closed)
      return undefined;

    const stream = this[kStream];
    const proxySocket = stream[kProxySocket];
    if (proxySocket === null)
      return stream[kProxySocket] = new Proxy(stream, proxySocketHandler);
    return proxySocket;
  }

  get connection() {
    return this.socket;
  }

  get stream() {
    return this[kStream];
  }

  get headersSent() {
    return this[kStream].headersSent;
  }

  get sendDate() {
    return this[kState].sendDate;
  }

  set sendDate(bool) {
    this[kState].sendDate = Boolean(bool);
  }

  get statusCode() {
    return this[kState].statusCode;
  }

  get writableCorked() {
    return this[kStream].writableCorked;
  }

  get writableHighWaterMark() {
    return this[kStream].writableHighWaterMark;
  }

  get writableFinished() {
    return this[kStream].writableFinished;
  }

  get writableLength() {
    return this[kStream].writableLength;
  }

  set statusCode(code) {
    code |= 0;
    if (code >= 100 && code < 200)
      throw new ERR_HTTP2_INFO_STATUS_NOT_ALLOWED();
    if (code < 100 || code > 599)
      throw new ERR_HTTP2_STATUS_INVALID(code);
    this[kState].statusCode = code;
  }

  setTrailer(name, value) {
    validateString(name, 'name');
    name = StringPrototypeToLowerCase(StringPrototypeTrim(name));
    assertValidHeader(name, value);
    this[kTrailers][name] = value;
  }

  addTrailers(headers) {
    const keys = ObjectKeys(headers);
    let key = '';
    for (let i = 0; i < keys.length; i++) {
      key = keys[i];
      this.setTrailer(key, headers[key]);
    }
  }

  getHeader(name) {
    validateString(name, 'name');
    name = StringPrototypeToLowerCase(StringPrototypeTrim(name));
    return this[kHeaders][name];
  }

  getHeaderNames() {
    return ObjectKeys(this[kHeaders]);
  }

  getHeaders() {
    const headers = ObjectCreate(null);
    return ObjectAssign(headers, this[kHeaders]);
  }

  hasHeader(name) {
    validateString(name, 'name');
    name = StringPrototypeToLowerCase(StringPrototypeTrim(name));
    return ObjectPrototypeHasOwnProperty(this[kHeaders], name);
  }

  removeHeader(name) {
    validateString(name, 'name');
    if (this[kStream].headersSent)
      throw new ERR_HTTP2_HEADERS_SENT();

    name = StringPrototypeToLowerCase(StringPrototypeTrim(name));

    if (name === 'date') {
      this[kState].sendDate = false;

      return;
    }

    delete this[kHeaders][name];
  }

  setHeader(name, value) {
    validateString(name, 'name');
    if (this[kStream].headersSent)
      throw new ERR_HTTP2_HEADERS_SENT();

    this[kSetHeader](name, value);
  }

  [kSetHeader](name, value) {
    name = StringPrototypeToLowerCase(StringPrototypeTrim(name));
    assertValidHeader(name, value);

    if (!isConnectionHeaderAllowed(name, value)) {
      return;
    }

    if (name[0] === ':')
      assertValidPseudoHeader(name);
    else if (!checkIsHttpToken(name))
      this.destroy(new ERR_INVALID_HTTP_TOKEN('Header name', name));

    this[kHeaders][name] = value;
  }

  get statusMessage() {
    statusMessageWarn();

    return '';
  }

  set statusMessage(msg) {
    statusMessageWarn();
  }

  flushHeaders() {
    const state = this[kState];
    if (!state.closed && !this[kStream].headersSent)
      this.writeHead(state.statusCode);
  }

  writeHead(statusCode, statusMessage, headers) {
    const state = this[kState];

    if (state.closed || this.stream.destroyed)
      return this;
    if (this[kStream].headersSent)
      throw new ERR_HTTP2_HEADERS_SENT();

    if (typeof statusMessage === 'string')
      statusMessageWarn();

    if (headers === undefined && typeof statusMessage === 'object')
      headers = statusMessage;

    let i;
    if (ArrayIsArray(headers)) {
      for (i = 0; i < headers.length; i++) {
        const header = headers[i];
        this[kSetHeader](header[0], header[1]);
      }
    } else if (typeof headers === 'object') {
      const keys = ObjectKeys(headers);
      let key = '';
      for (i = 0; i < keys.length; i++) {
        key = keys[i];
        this[kSetHeader](key, headers[key]);
      }
    }

    state.statusCode = statusCode;
    this[kBeginSend]();

    return this;
  }

  cork() {
    this[kStream].cork();
  }

  uncork() {
    this[kStream].uncork();
  }

  write(chunk, encoding, cb) {
    const state = this[kState];

    if (typeof encoding === 'function') {
      cb = encoding;
      encoding = 'utf8';
    }

    let err;
    if (state.ending) {
      err = new ERR_STREAM_WRITE_AFTER_END();
    } else if (state.closed) {
      err = new ERR_HTTP2_INVALID_STREAM();
    } else if (state.destroyed) {
      return false;
    }

    if (err) {
      if (typeof cb === 'function')
        process.nextTick(cb, err);
      this.destroy(err);
      return false;
    }

    const stream = this[kStream];
    if (!stream.headersSent)
      this.writeHead(state.statusCode);
    return stream.write(chunk, encoding, cb);
  }

  end(chunk, encoding, cb) {
    const stream = this[kStream];
    const state = this[kState];

    if (typeof chunk === 'function') {
      cb = chunk;
      chunk = null;
    } else if (typeof encoding === 'function') {
      cb = encoding;
      encoding = 'utf8';
    }

    if ((state.closed || state.ending) &&
        state.headRequest === stream.headRequest) {
      if (typeof cb === 'function') {
        process.nextTick(cb);
      }
      return this;
    }

    if (chunk !== null && chunk !== undefined)
      this.write(chunk, encoding);

    state.headRequest = stream.headRequest;
    state.ending = true;

    if (typeof cb === 'function') {
      if (stream.writableEnded)
        this.once('finish', cb);
      else
        stream.once('finish', cb);
    }

    if (!stream.headersSent)
      this.writeHead(this[kState].statusCode);

    if (this[kState].closed || stream.destroyed)
      ReflectApply(onStreamCloseResponse, stream, []);
    else
      stream.end();

    return this;
  }

  destroy(err) {
    if (this[kState].destroyed)
      return;

    this[kState].destroyed = true;
    this[kStream].destroy(err);
  }

  setTimeout(msecs, callback) {
    if (this[kState].closed)
      return;
    this[kStream].setTimeout(msecs, callback);
  }

  createPushResponse(headers, callback) {
    validateCallback(callback);
    if (this[kState].closed) {
      process.nextTick(callback, new ERR_HTTP2_INVALID_STREAM());
      return;
    }
    this[kStream].pushStream(headers, {}, (err, stream, headers, options) => {
      if (err) {
        callback(err);
        return;
      }
      callback(null, new Http2ServerResponse(stream));
    });
  }

  [kBeginSend]() {
    const state = this[kState];
    const headers = this[kHeaders];
    headers[HTTP2_HEADER_STATUS] = state.statusCode;
    const options = {
      endStream: state.ending,
      waitForTrailers: true,
      sendDate: state.sendDate
    };
    this[kStream].respond(headers, options);
  }

  // TODO doesn't support callbacks
  writeContinue() {
    const stream = this[kStream];
    if (stream.headersSent || this[kState].closed)
      return false;
    stream.additionalHeaders({
      [HTTP2_HEADER_STATUS]: HTTP_STATUS_CONTINUE
    });
    return true;
  }
}

function onServerStream(ServerRequest, ServerResponse,
                        stream, headers, flags, rawHeaders) {
  const server = this;
  const request = new ServerRequest(stream, headers, undefined, rawHeaders);
  const response = new ServerResponse(stream);

  // Check for the CONNECT method
  const method = headers[HTTP2_HEADER_METHOD];
  if (method === 'CONNECT') {
    if (!server.emit('connect', request, response)) {
      response.statusCode = HTTP_STATUS_METHOD_NOT_ALLOWED;
      response.end();
    }
    return;
  }

  // Check for Expectations
  if (headers.expect !== undefined) {
    if (headers.expect === '100-continue') {
      if (server.listenerCount('checkContinue')) {
        server.emit('checkContinue', request, response);
      } else {
        response.writeContinue();
        server.emit('request', request, response);
      }
    } else if (server.listenerCount('checkExpectation')) {
      server.emit('checkExpectation', request, response);
    } else {
      response.statusCode = HTTP_STATUS_EXPECTATION_FAILED;
      response.end();
    }
    return;
  }

  server.emit('request', request, response);
}

module.exports = {
  onServerStream,
  Http2ServerRequest,
  Http2ServerResponse,
};
