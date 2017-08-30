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
const kTrailers = Symbol('trailers');

let statusMessageWarned = false;

// Defines and implements an API compatibility layer on top of the core
// HTTP/2 implementation, intended to provide an interface that is as
// close as possible to the current require('http') API

function assertValidHeader(name, value) {
  if (isPseudoHeader(name))
    throw new errors.Error('ERR_HTTP2_PSEUDOHEADER_NOT_ALLOWED');
  if (value === undefined || value === null)
    throw new errors.TypeError('ERR_HTTP2_INVALID_HEADER_VALUE');
}

function isPseudoHeader(name) {
  switch (name) {
    case ':status':
      return true;
    case ':method':
      return true;
    case ':path':
      return true;
    case ':authority':
      return true;
    case ':scheme':
      return true;
    default:
      return false;
  }
}

function onStreamData(chunk) {
  const request = this[kRequest];
  if (!request.push(chunk))
    this.pause();
}

function onStreamEnd() {
  // Cause the request stream to end as well.
  const request = this[kRequest];
  request.push(null);
}

function onStreamError(error) {
  // this is purposefully left blank
  //
  // errors in compatibility mode are
  // not forwarded to the request
  // and response objects. However,
  // they are forwarded to 'clientError'
  // on the server by Http2Stream
}

function onRequestPause() {
  const stream = this[kStream];
  stream.pause();
}

function onRequestResume() {
  const stream = this[kStream];
  stream.resume();
}

function onRequestDrain() {
  if (this.isPaused())
    this.resume();
}

function onStreamResponseDrain() {
  const response = this[kResponse];
  response.emit('drain');
}

function onStreamClosedRequest() {
  const req = this[kRequest];
  req.push(null);
}

function onStreamClosedResponse() {
  const res = this[kResponse];
  res.writable = false;
  res.emit('finish');
}

function onAborted(hadError, code) {
  if ((this.writable) ||
      (this._readableState && !this._readableState.ended)) {
    this.emit('aborted', hadError, code);
  }
}

class Http2ServerRequest extends Readable {
  constructor(stream, headers, options) {
    super(options);
    this[kState] = {
      statusCode: null,
      closed: false,
      closedCode: constants.NGHTTP2_NO_ERROR
    };
    this[kHeaders] = headers;
    this[kStream] = stream;
    stream[kRequest] = this;

    // Pause the stream..
    stream.pause();
    stream.on('data', onStreamData);
    stream.on('end', onStreamEnd);
    stream.on('error', onStreamError);
    stream.on('close', onStreamClosedRequest);
    stream.on('aborted', onAborted.bind(this));
    const onfinish = this[kFinish].bind(this);
    stream.on('streamClosed', onfinish);
    stream.on('finish', onfinish);
    this.on('pause', onRequestPause);
    this.on('resume', onRequestResume);
    this.on('drain', onRequestDrain);
  }

  get closed() {
    const state = this[kState];
    return Boolean(state.closed);
  }

  get code() {
    const state = this[kState];
    return Number(state.closedCode);
  }

  get stream() {
    return this[kStream];
  }

  get statusCode() {
    return this[kState].statusCode;
  }

  get headers() {
    return this[kHeaders];
  }

  get rawHeaders() {
    const headers = this[kHeaders];
    if (headers === undefined)
      return [];
    const tuples = Object.entries(headers);
    const flattened = Array.prototype.concat.apply([], tuples);
    return flattened.map(String);
  }

  get trailers() {
    return this[kTrailers];
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
    return this.stream.session.socket;
  }

  get connection() {
    return this.socket;
  }

  _read(nread) {
    const stream = this[kStream];
    if (stream) {
      stream.resume();
    } else {
      this.emit('error', new errors.Error('ERR_HTTP2_STREAM_CLOSED'));
    }
  }

  get method() {
    const headers = this[kHeaders];
    if (headers === undefined)
      return;
    return headers[constants.HTTP2_HEADER_METHOD];
  }

  get authority() {
    const headers = this[kHeaders];
    if (headers === undefined)
      return;
    return headers[constants.HTTP2_HEADER_AUTHORITY];
  }

  get scheme() {
    const headers = this[kHeaders];
    if (headers === undefined)
      return;
    return headers[constants.HTTP2_HEADER_SCHEME];
  }

  get url() {
    return this.path;
  }

  set url(url) {
    this.path = url;
  }

  get path() {
    const headers = this[kHeaders];
    if (headers === undefined)
      return;
    return headers[constants.HTTP2_HEADER_PATH];
  }

  set path(path) {
    let headers = this[kHeaders];
    if (headers === undefined)
      headers = this[kHeaders] = Object.create(null);
    headers[constants.HTTP2_HEADER_PATH] = path;
  }

  setTimeout(msecs, callback) {
    const stream = this[kStream];
    if (stream === undefined) return;
    stream.setTimeout(msecs, callback);
  }

  [kFinish](code) {
    const state = this[kState];
    if (state.closed)
      return;
    state.closedCode = code;
    state.closed = true;
    this.push(null);
    this[kStream] = undefined;
  }
}

class Http2ServerResponse extends Stream {
  constructor(stream, options) {
    super(options);
    this[kState] = {
      sendDate: true,
      statusCode: constants.HTTP_STATUS_OK,
      headerCount: 0,
      trailerCount: 0,
      closed: false,
      closedCode: constants.NGHTTP2_NO_ERROR
    };
    this[kStream] = stream;
    stream[kResponse] = this;
    this.writable = true;
    stream.on('drain', onStreamResponseDrain);
    stream.on('close', onStreamClosedResponse);
    const onfinish = this[kFinish].bind(this);
    stream.on('streamClosed', onfinish);
    stream.on('finish', onfinish);
  }

  get finished() {
    const stream = this[kStream];
    return stream === undefined || stream._writableState.ended;
  }

  get closed() {
    const state = this[kState];
    return Boolean(state.closed);
  }

  get code() {
    const state = this[kState];
    return Number(state.closedCode);
  }

  get stream() {
    return this[kStream];
  }

  get headersSent() {
    const stream = this[kStream];
    return stream.headersSent;
  }

  get sendDate() {
    return Boolean(this[kState].sendDate);
  }

  set sendDate(bool) {
    this[kState].sendDate = Boolean(bool);
  }

  get statusCode() {
    return this[kState].statusCode;
  }

  set statusCode(code) {
    const state = this[kState];
    code |= 0;
    if (code >= 100 && code < 200)
      throw new errors.RangeError('ERR_HTTP2_INFO_STATUS_NOT_ALLOWED');
    if (code < 200 || code > 599)
      throw new errors.RangeError('ERR_HTTP2_STATUS_INVALID', code);
    state.statusCode = code;
  }

  addTrailers(headers) {
    let trailers = this[kTrailers];
    const keys = Object.keys(headers);
    if (keys.length === 0)
      return;
    if (trailers === undefined)
      trailers = this[kTrailers] = Object.create(null);
    for (var i = 0; i < keys.length; i++) {
      trailers[keys[i]] = String(headers[keys[i]]);
    }
  }

  getHeader(name) {
    const headers = this[kHeaders];
    if (headers === undefined)
      return;
    name = String(name).trim().toLowerCase();
    return headers[name];
  }

  getHeaderNames() {
    const headers = this[kHeaders];
    if (headers === undefined)
      return [];
    return Object.keys(headers);
  }

  getHeaders() {
    const headers = this[kHeaders];
    return Object.assign({}, headers);
  }

  hasHeader(name) {
    const headers = this[kHeaders];
    if (headers === undefined)
      return false;
    name = String(name).trim().toLowerCase();
    return Object.prototype.hasOwnProperty.call(headers, name);
  }

  removeHeader(name) {
    const headers = this[kHeaders];
    if (headers === undefined)
      return;
    name = String(name).trim().toLowerCase();
    delete headers[name];
  }

  setHeader(name, value) {
    name = String(name).trim().toLowerCase();
    assertValidHeader(name, value);
    let headers = this[kHeaders];
    if (headers === undefined)
      headers = this[kHeaders] = Object.create(null);
    headers[name] = String(value);
  }

  get statusMessage() {
    if (statusMessageWarned === false) {
      process.emitWarning(
        'Status message is not supported by HTTP/2 (RFC7540 8.1.2.4)',
        'UnsupportedWarning'
      );
      statusMessageWarned = true;
    }

    return '';
  }

  flushHeaders() {
    if (this[kStream].headersSent === false)
      this[kBeginSend]();
  }

  writeHead(statusCode, statusMessage, headers) {
    if (typeof statusMessage === 'string' && statusMessageWarned === false) {
      process.emitWarning(
        'Status message is not supported by HTTP/2 (RFC7540 8.1.2.4)',
        'UnsupportedWarning'
      );
      statusMessageWarned = true;
    }
    if (headers === undefined && typeof statusMessage === 'object') {
      headers = statusMessage;
    }

    const stream = this[kStream];
    if (stream.headersSent === true) {
      throw new errors.Error('ERR_HTTP2_INFO_HEADERS_AFTER_RESPOND');
    }

    if (headers) {
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
      if (cb)
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
      encoding = 'utf8';
    } else if (typeof encoding === 'function') {
      cb = encoding;
      encoding = 'utf8';
    }
    if (chunk !== null && chunk !== undefined) {
      this.write(chunk, encoding);
    }

    if (typeof cb === 'function' && stream !== undefined) {
      stream.once('finish', cb);
    }

    this[kBeginSend]({ endStream: true });

    if (stream !== undefined) {
      stream.end();
    }
  }

  destroy(err) {
    const stream = this[kStream];
    if (stream === undefined) {
      // nothing to do, already closed
      return;
    }
    stream.destroy(err);
  }

  setTimeout(msecs, callback) {
    const stream = this[kStream];
    if (stream === undefined) return;
    stream.setTimeout(msecs, callback);
  }

  createPushResponse(headers, callback) {
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
    options = options || Object.create(null);
    if (stream !== undefined && stream.headersSent === false) {
      const state = this[kState];
      const headers = this[kHeaders] || Object.create(null);
      headers[constants.HTTP2_HEADER_STATUS] = state.statusCode;
      if (stream.finished === true)
        options.endStream = true;
      options.getTrailers = (trailers) => {
        Object.assign(trailers, this[kTrailers]);
      };
      if (stream.destroyed === false) {
        stream.respond(headers, options);
      }
    }
  }

  [kFinish](code) {
    const state = this[kState];
    if (state.closed)
      return;
    state.closedCode = code;
    state.closed = true;
    this.end();
    this[kStream] = undefined;
    this.emit('finish');
  }

  // TODO doesn't support callbacks
  writeContinue() {
    const stream = this[kStream];
    if (stream === undefined) return false;
    this[kStream].additionalHeaders({
      [constants.HTTP2_HEADER_STATUS]: constants.HTTP_STATUS_CONTINUE
    });
    return true;
  }
}

function onServerStream(stream, headers, flags) {
  const server = this;
  const request = new Http2ServerRequest(stream, headers);
  const response = new Http2ServerResponse(stream);

  // Check for the CONNECT method
  const method = headers[constants.HTTP2_HEADER_METHOD];
  if (method === 'CONNECT') {
    if (!server.emit('connect', request, response)) {
      response.statusCode = constants.HTTP_STATUS_METHOD_NOT_ALLOWED;
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
      response.statusCode = constants.HTTP_STATUS_EXPECTATION_FAILED;
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
