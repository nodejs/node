'use strict';

const Stream = require('stream');
const Readable = Stream.Readable;
const binding = process.binding('http2');
const constants = binding.constants;
const errors = require('internal/errors');

const kFinish = Symbol('finish');
const kBeginSend = Symbol('begin-send');
const kState = Symbol('state');
const kStream = Symbol('stream');
const kRequest = Symbol('request');
const kResponse = Symbol('response');
const kHeaders = Symbol('headers');
const kRawHeaders = Symbol('rawHeaders');
const kTrailers = Symbol('trailers');
const kRawTrailers = Symbol('rawTrailers');

const {
  NGHTTP2_NO_ERROR,

  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_STATUS,

  HTTP_STATUS_CONTINUE,
  HTTP_STATUS_EXPECTATION_FAILED,
  HTTP_STATUS_METHOD_NOT_ALLOWED,
  HTTP_STATUS_OK
} = constants;

let statusMessageWarned = false;

// Defines and implements an API compatibility layer on top of the core
// HTTP/2 implementation, intended to provide an interface that is as
// close as possible to the current require('http') API

function assertValidHeader(name, value) {
  if (name === '')
    throw new errors.TypeError('ERR_INVALID_HTTP_TOKEN', 'Header name', name);
  if (isPseudoHeader(name))
    throw new errors.Error('ERR_HTTP2_PSEUDOHEADER_NOT_ALLOWED');
  if (value === undefined || value === null)
    throw new errors.TypeError('ERR_HTTP2_INVALID_HEADER_VALUE');
}

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

function onStreamData(chunk) {
  if (!this[kRequest].push(chunk))
    this.pause();
}

function onStreamTrailers(trailers, flags, rawTrailers) {
  const request = this[kRequest];
  Object.assign(request[kTrailers], trailers);
  request[kRawTrailers].push(...rawTrailers);
}

function onStreamEnd() {
  // Cause the request stream to end as well.
  this[kRequest].push(null);
}

function onStreamError(error) {
  // this is purposefully left blank
  //
  // errors in compatibility mode are
  // not forwarded to the request
  // and response objects. However,
  // they are forwarded to 'streamError'
  // on the server by Http2Stream
}

function onRequestPause() {
  const stream = this[kStream];
  if (stream)
    stream.pause();
}

function onRequestResume() {
  const stream = this[kStream];
  if (stream)
    stream.resume();
}

function onStreamDrain() {
  this[kResponse].emit('drain');
}

// TODO Http2Stream does not emit 'close'
function onStreamClosedRequest() {
  this[kRequest].push(null);
}

// TODO Http2Stream does not emit 'close'
function onStreamClosedResponse() {
  this[kResponse].emit('finish');
}

function onStreamAbortedRequest(hadError, code) {
  const request = this[kRequest];
  if (request[kState].closed === false) {
    request.emit('aborted', hadError, code);
    request.emit('close');
  }
}

function onStreamAbortedResponse() {
  const response = this[kResponse];
  if (response[kState].closed === false) {
    response.emit('close');
  }
}

function resumeStream(stream) {
  stream.resume();
}

class Http2ServerRequest extends Readable {
  constructor(stream, headers, options, rawHeaders) {
    super(options);
    this[kState] = {
      closed: false,
      closedCode: NGHTTP2_NO_ERROR
    };
    this[kHeaders] = headers;
    this[kRawHeaders] = rawHeaders;
    this[kTrailers] = {};
    this[kRawTrailers] = [];
    this[kStream] = stream;
    stream[kRequest] = this;

    // Pause the stream..
    stream.pause();
    stream.on('data', onStreamData);
    stream.on('trailers', onStreamTrailers);
    stream.on('end', onStreamEnd);
    stream.on('error', onStreamError);
    stream.on('close', onStreamClosedRequest);
    stream.on('aborted', onStreamAbortedRequest);
    const onfinish = this[kFinish].bind(this);
    stream.on('streamClosed', onfinish);
    stream.on('finish', onfinish);
    this.on('pause', onRequestPause);
    this.on('resume', onRequestResume);
  }

  get closed() {
    return this[kState].closed;
  }

  get code() {
    return this[kState].closedCode;
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
    if (stream === undefined)
      return;
    return stream.session.socket;
  }

  get connection() {
    return this.socket;
  }

  _read(nread) {
    const stream = this[kStream];
    if (stream !== undefined) {
      process.nextTick(resumeStream, stream);
    } else {
      this.emit('error', new errors.Error('ERR_HTTP2_STREAM_CLOSED'));
    }
  }

  get method() {
    return this[kHeaders][HTTP2_HEADER_METHOD];
  }

  get authority() {
    return this[kHeaders][HTTP2_HEADER_AUTHORITY];
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
    if (this[kState].closed)
      return;
    this[kStream].setTimeout(msecs, callback);
  }

  [kFinish](code) {
    const state = this[kState];
    if (state.closed)
      return;
    if (code !== undefined)
      state.closedCode = Number(code);
    state.closed = true;
    this.push(null);
    process.nextTick(() => (this[kStream] = undefined));
  }
}

class Http2ServerResponse extends Stream {
  constructor(stream, options) {
    super(options);
    this[kState] = {
      sendDate: true,
      statusCode: HTTP_STATUS_OK,
      closed: false,
      closedCode: NGHTTP2_NO_ERROR
    };
    this[kHeaders] = Object.create(null);
    this[kTrailers] = Object.create(null);
    this[kStream] = stream;
    stream[kResponse] = this;
    this.writable = true;
    stream.on('drain', onStreamDrain);
    stream.on('close', onStreamClosedResponse);
    stream.on('aborted', onStreamAbortedResponse);
    const onfinish = this[kFinish].bind(this);
    stream.on('streamClosed', onfinish);
    stream.on('finish', onfinish);
  }

  get finished() {
    const stream = this[kStream];
    return stream === undefined || stream._writableState.ended;
  }

  get closed() {
    return this[kState].closed;
  }

  get code() {
    return this[kState].closedCode;
  }

  get stream() {
    return this[kStream];
  }

  get headersSent() {
    const stream = this[kStream];
    return stream !== undefined ? stream.headersSent : this[kState].headersSent;
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

  set statusCode(code) {
    code |= 0;
    if (code >= 100 && code < 200)
      throw new errors.RangeError('ERR_HTTP2_INFO_STATUS_NOT_ALLOWED');
    if (code < 100 || code > 599)
      throw new errors.RangeError('ERR_HTTP2_STATUS_INVALID', code);
    this[kState].statusCode = code;
  }

  setTrailer(name, value) {
    if (typeof name !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'name', 'string');

    name = name.trim().toLowerCase();
    assertValidHeader(name, value);
    this[kTrailers][name] = String(value);
  }

  addTrailers(headers) {
    const keys = Object.keys(headers);
    let key = '';
    for (var i = 0; i < keys.length; i++) {
      key = keys[i];
      this.setTrailer(key, headers[key]);
    }
  }

  getHeader(name) {
    if (typeof name !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'name', 'string');

    name = name.trim().toLowerCase();
    return this[kHeaders][name];
  }

  getHeaderNames() {
    return Object.keys(this[kHeaders]);
  }

  getHeaders() {
    return Object.assign({}, this[kHeaders]);
  }

  hasHeader(name) {
    if (typeof name !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'name', 'string');

    name = name.trim().toLowerCase();
    return Object.prototype.hasOwnProperty.call(this[kHeaders], name);
  }

  removeHeader(name) {
    if (typeof name !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'name', 'string');

    name = name.trim().toLowerCase();
    delete this[kHeaders][name];
  }

  setHeader(name, value) {
    if (typeof name !== 'string')
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'name', 'string');

    name = name.trim().toLowerCase();
    assertValidHeader(name, value);
    this[kHeaders][name] = String(value);
  }

  get statusMessage() {
    statusMessageWarn();

    return '';
  }

  set statusMessage(msg) {
    statusMessageWarn();
  }

  flushHeaders() {
    const stream = this[kStream];
    if (stream !== undefined && stream.headersSent === false)
      this[kBeginSend]();
  }

  writeHead(statusCode, statusMessage, headers) {
    if (typeof statusMessage === 'string') {
      statusMessageWarn();
    }

    if (headers === undefined && typeof statusMessage === 'object') {
      headers = statusMessage;
    }

    const stream = this[kStream];
    if (stream === undefined) {
      throw new errors.Error('ERR_HTTP2_STREAM_CLOSED');
    }
    if (stream.headersSent === true) {
      throw new errors.Error('ERR_HTTP2_INFO_HEADERS_AFTER_RESPOND');
    }

    if (typeof headers === 'object') {
      const keys = Object.keys(headers);
      let key = '';
      for (var i = 0; i < keys.length; i++) {
        key = keys[i];
        this.setHeader(key, headers[key]);
      }
    }

    this.statusCode = statusCode;
    this[kBeginSend]();
  }

  write(chunk, encoding, cb) {
    const stream = this[kStream];

    if (typeof encoding === 'function') {
      cb = encoding;
      encoding = 'utf8';
    }

    if (stream === undefined) {
      const err = new errors.Error('ERR_HTTP2_STREAM_CLOSED');
      if (typeof cb === 'function')
        process.nextTick(cb, err);
      else
        throw err;
      return;
    }
    this[kBeginSend]();
    return stream.write(chunk, encoding, cb);
  }

  end(chunk, encoding, cb) {
    const stream = this[kStream];

    if (typeof chunk === 'function') {
      cb = chunk;
      chunk = null;
    } else if (typeof encoding === 'function') {
      cb = encoding;
      encoding = 'utf8';
    }
    if (this.finished === true) {
      return false;
    }
    if (chunk !== null && chunk !== undefined) {
      this.write(chunk, encoding);
    }

    if (typeof cb === 'function') {
      stream.once('finish', cb);
    }

    this[kBeginSend]({ endStream: true });
    stream.end();
  }

  destroy(err) {
    if (this[kState].closed)
      return;
    this[kStream].destroy(err);
  }

  setTimeout(msecs, callback) {
    const stream = this[kStream];
    if (this[kState].closed)
      return;
    stream.setTimeout(msecs, callback);
  }

  createPushResponse(headers, callback) {
    if (typeof callback !== 'function')
      throw new errors.TypeError('ERR_INVALID_CALLBACK');
    const stream = this[kStream];
    if (stream === undefined) {
      process.nextTick(callback, new errors.Error('ERR_HTTP2_STREAM_CLOSED'));
      return;
    }
    stream.pushStream(headers, {}, function(stream, headers, options) {
      const response = new Http2ServerResponse(stream);
      callback(null, response);
    });
  }

  [kBeginSend](options) {
    const stream = this[kStream];
    if (stream !== undefined &&
        stream.destroyed === false &&
        stream.headersSent === false) {
      const headers = this[kHeaders];
      headers[HTTP2_HEADER_STATUS] = this[kState].statusCode;
      options = options || Object.create(null);
      options.getTrailers = (trailers) => {
        Object.assign(trailers, this[kTrailers]);
      };
      stream.respond(headers, options);
    }
  }

  [kFinish](code) {
    const state = this[kState];
    if (state.closed)
      return;
    if (code !== undefined)
      state.closedCode = Number(code);
    state.closed = true;
    state.headersSent = this[kStream].headersSent;
    this.end();
    process.nextTick(() => (this[kStream] = undefined));
    this.emit('finish');
  }

  // TODO doesn't support callbacks
  writeContinue() {
    const stream = this[kStream];
    if (stream === undefined ||
        stream.headersSent === true ||
        stream.destroyed === true) {
      return false;
    }
    this[kStream].additionalHeaders({
      [HTTP2_HEADER_STATUS]: HTTP_STATUS_CONTINUE
    });
    return true;
  }
}

function onServerStream(stream, headers, flags, rawHeaders) {
  const server = this;
  const request = new Http2ServerRequest(stream, headers, undefined,
                                         rawHeaders);
  const response = new Http2ServerResponse(stream);

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
