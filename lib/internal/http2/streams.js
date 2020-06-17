'use strict';

const {
  ArrayIsArray,
  MathMin,
  ObjectAssign,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectPrototypeHasOwnProperty,
  Symbol,
} = primordials;

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const assert = require('assert');
const fs = require('fs');
const { Duplex } = require('stream');
const { setImmediate } = require('timers');

const {
  assertIsObject,
  assertValidPseudoHeaderResponse,
  assertValidPseudoHeaderTrailer,
  callTimeout,
  getStreamState,
  kMaxInt,
  kSensitiveHeaders,
  kState,
  kRequest,
  kProxySocket,
  mapToHeaders,
  NghttpError,
  sessionName,
  setAndValidatePriorityOptions,
} = require('internal/http2/util');
const {
  defaultTriggerAsyncIdScope,
  symbols: {
    async_id_symbol,
    owner_symbol,
  },
} = require('internal/async_hooks');
const {
  writeGeneric,
  writevGeneric,
  onStreamRead,
  kAfterAsyncWrite,
  kMaybeDestroy,
  kUpdateTimer,
  kHandle,
  kSession,
  setStreamTimeout
} = require('internal/stream_base_commons');
const { kTimeout } = require('internal/timers');
const { format } = require('internal/util/inspect');
const {
  codes: {
    ERR_HTTP2_HEADERS_AFTER_RESPOND,
    ERR_HTTP2_HEADERS_SENT,
    ERR_HTTP2_INVALID_INFO_STATUS,
    ERR_HTTP2_INVALID_STREAM,
    ERR_HTTP2_NESTED_PUSH,
    ERR_HTTP2_OUT_OF_STREAMS,
    ERR_HTTP2_PAYLOAD_FORBIDDEN,
    ERR_HTTP2_PUSH_DISABLED,
    ERR_HTTP2_SEND_FILE,
    ERR_HTTP2_SEND_FILE_NOSEEK,
    ERR_HTTP2_STATUS_101,
    ERR_HTTP2_STATUS_INVALID,
    ERR_HTTP2_STREAM_ERROR,
    ERR_HTTP2_TRAILERS_ALREADY_SENT,
    ERR_HTTP2_TRAILERS_NOT_READY,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_CALLBACK,
    ERR_INVALID_OPT_VALUE,
  },
} = require('internal/errors');
const { validateInteger } = require('internal/validators');
const fsPromisesInternal = require('internal/fs/promises');
const { utcDate } = require('internal/http');
let debug = require('internal/util/debuglog').debuglog('http2', (fn) => {
  debug = fn;
});

const binding = internalBinding('http2');
const { FileHandle } = internalBinding('fs');
const {
  ShutdownWrap,
  kReadBytesOrError,
  streamBaseState
} = internalBinding('stream_wrap');
const { UV_EOF } = internalBinding('uv');
const { StreamPipe } = internalBinding('stream_pipe');

const kID = Symbol('id');
const kInit = Symbol('init');
const kInfoHeaders = Symbol('sent-info-headers');
const kOwner = owner_symbol;
const kProceed = Symbol('proceed');
const kProtocol = Symbol('protocol');
const kAuthority = Symbol('authority');
const kNativeFields = Symbol('kNativeFields');
const kSentHeaders = Symbol('sent-headers');
const kSentTrailers = Symbol('sent-trailers');
const kWriteGeneric = Symbol('write-generic');
const kType = Symbol('type');

const {
  constants,
  nameForErrorCode,
  kSessionFrameErrorListenerCount,
  kSessionPriorityListenerCount,
} = binding;

const {
  NGHTTP2_CANCEL,
  NGHTTP2_INTERNAL_ERROR,
  NGHTTP2_NO_ERROR,
  NGHTTP2_SESSION_SERVER,
  NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE,
  NGHTTP2_ERR_STREAM_CLOSED,

  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_DATE,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_CONTENT_LENGTH,

  HTTP2_METHOD_GET,
  HTTP2_METHOD_HEAD,

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
const STREAM_FLAGS_HAS_TRAILERS = 0x20;

const kNoRstStream = 0;
const kSubmitRstStream = 1;
const kForceRstStream = 2;

// TODO(addaleax): See if this can be made more efficient by figuring out
// whether debugging is enabled before we perform any further steps. Currently,
// this seems pretty fast, though.
function debugStream(id, sessionType, message, ...args) {
  debug('Http2Stream %s [Http2Session %s]: ' + message,
        id, sessionName(sessionType), ...args);
}

function debugStreamObj(stream, message, ...args) {
  debugStream(stream[kID], stream[kSession][kType], message, ...args);
}

// An Http2Stream is a Duplex stream that is backed by a
// node::http2::Http2Stream handle implementing StreamBase.
class Http2Stream extends Duplex {
  constructor(session, options) {
    options.allowHalfOpen = true;
    options.decodeStrings = false;
    options.autoDestroy = false;
    super(options);
    this[async_id_symbol] = -1;

    // Corking the stream automatically allows writes to happen
    // but ensures that those are buffered until the handle has
    // been assigned.
    this.cork();
    this[kSession] = session;
    session[kState].pendingStreams.add(this);

    // Allow our logic for determining whether any reads have happened to
    // work in all situations. This is similar to what we do in _http_incoming.
    this._readableState.readingMore = true;

    this[kTimeout] = null;

    this[kState] = {
      didRead: false,
      flags: STREAM_FLAGS_PENDING,
      rstCode: NGHTTP2_NO_ERROR,
      writeQueueSize: 0,
      trailersReady: false,
      endAfterHeaders: false
    };

    // Fields used by the compat API to avoid megamorphisms.
    this[kRequest] = null;
    this[kProxySocket] = null;

    this.on('pause', streamOnPause);

    this.on('newListener', streamListenerAdded);
    this.on('removeListener', streamListenerRemoved);
  }

  [kUpdateTimer]() {
    if (this.destroyed)
      return;
    if (this[kTimeout])
      this[kTimeout].refresh();
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
    handle.onread = onStreamRead;
    this.uncork();
    this.emit('ready');
  }

  [kInspect](depth, opts) {
    if (typeof depth === 'number' && depth < 0)
      return this;

    const obj = {
      id: this[kID] || '<pending>',
      closed: this.closed,
      destroyed: this.destroyed,
      state: this.state,
      readableState: this._readableState,
      writableState: this._writableState
    };
    return `Http2Stream ${format(obj)}`;
  }

  get bufferSize() {
    // `bufferSize` properties of `net.Socket` are `undefined` when
    // their `_handle` are falsy. Here we avoid the behavior.
    return this[kState].writeQueueSize + this.writableLength;
  }

  get endAfterHeaders() {
    return this[kState].endAfterHeaders;
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
    callTimeout(this, kSession);
  }

  // True if the HEADERS frame has been sent
  get headersSent() {
    return !!(this[kState].flags & STREAM_FLAGS_HEADERS_SENT);
  }

  // True if the Http2Stream was aborted abnormally.
  get aborted() {
    return !!(this[kState].flags & STREAM_FLAGS_ABORTED);
  }

  // True if dealing with a HEAD request
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
    assert.fail('Implementors MUST implement this. Please report this as a ' +
                'bug in Node.js');
  }

  [kAfterAsyncWrite]({ bytes }) {
    this[kState].writeQueueSize -= bytes;

    if (this.session !== undefined)
      this.session[kState].writeQueueSize -= bytes;
  }

  [kWriteGeneric](writev, data, encoding, cb) {
    // When the Http2Stream is first created, it is corked until the
    // handle and the stream ID is assigned. However, if the user calls
    // uncork() before that happens, the Duplex will attempt to pass
    // writes through. Those need to be queued up here.
    if (this.pending) {
      this.once(
        'ready',
        this[kWriteGeneric].bind(this, writev, data, encoding, cb)
      );
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

    let req;

    // writeGeneric does not destroy on error and we cannot enable autoDestroy,
    // so make sure to destroy on error.
    const callback = (err) => {
      if (err) {
        this.destroy(err);
      }
      cb(err);
    };

    if (writev)
      req = writevGeneric(this, data, callback);
    else
      req = writeGeneric(this, data, encoding, callback);

    trackWriteState(this, req.bytes);
  }

  _write(data, encoding, cb) {
    this[kWriteGeneric](false, data, encoding, cb);
  }

  _writev(data, cb) {
    this[kWriteGeneric](true, data, '', cb);
  }

  _final(cb) {
    const handle = this[kHandle];
    if (this.pending) {
      this.once('ready', () => this._final(cb));
    } else if (handle !== undefined) {
      debugStreamObj(this, '_final shutting down');
      const req = new ShutdownWrap();
      req.oncomplete = afterShutdown;
      req.callback = cb;
      req.handle = handle;
      const err = handle.shutdown(req);
      if (err === 1)  // synchronous finish
        return afterShutdown.call(req, 0);
    } else {
      cb();
    }
  }

  _read(nread) {
    if (this.destroyed) {
      this.push(null);
      return;
    }
    if (!this[kState].didRead) {
      this._readableState.readingMore = false;
      this[kState].didRead = true;
    }
    if (!this.pending) {
      streamOnResume.call(this);
    } else {
      this.once('ready', streamOnResume);
    }
  }

  priority(options) {
    if (this.destroyed)
      throw new ERR_HTTP2_INVALID_STREAM();

    assertIsObject(options, 'options');
    options = { ...options };
    setAndValidatePriorityOptions(options);

    const priorityFn = submitPriority.bind(this, options);

    // If the handle has not yet been assigned, queue up the priority
    // frame to be sent as soon as the ready event is emitted.
    if (this.pending) {
      this.once('ready', priorityFn);
      return;
    }
    priorityFn();
  }

  sendTrailers(headers) {
    if (this.destroyed || this.closed)
      throw new ERR_HTTP2_INVALID_STREAM();
    if (this[kSentTrailers])
      throw new ERR_HTTP2_TRAILERS_ALREADY_SENT();
    if (!this[kState].trailersReady)
      throw new ERR_HTTP2_TRAILERS_NOT_READY();

    assertIsObject(headers, 'headers');
    headers = ObjectAssign(ObjectCreate(null), headers);

    debugStreamObj(this, 'sending trailers');

    this[kUpdateTimer]();

    const headersList = mapToHeaders(headers, assertValidPseudoHeaderTrailer);
    this[kSentTrailers] = headers;

    // Send the trailers in setImmediate so we don't do it on nghttp2 stack.
    setImmediate(finishSendTrailers, this, headersList);
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
    validateInteger(code, 'code', 0, kMaxInt);

    if (callback !== undefined && typeof callback !== 'function')
      throw new ERR_INVALID_CALLBACK(callback);

    if (this.closed)
      return;

    if (callback !== undefined)
      this.once('close', callback);

    closeStream(this, code);
  }

  // Called by this.destroy().
  // * Will submit an RST stream to shutdown the stream if necessary.
  //   This will cause the internal resources to be released.
  // * Then cleans up the resources on the js side
  _destroy(err, callback) {
    const session = this[kSession];
    const handle = this[kHandle];
    const id = this[kID];

    debugStream(this[kID] || 'pending', session[kType], 'destroying stream');

    const state = this[kState];
    const sessionCode = session[kState].goawayCode ||
                        session[kState].destroyCode;
    const code = err != null ?
      sessionCode || NGHTTP2_INTERNAL_ERROR :
      state.rstCode || sessionCode;
    const hasHandle = handle !== undefined;

    if (!this.closed)
      closeStream(this, code, hasHandle ? kForceRstStream : kNoRstStream);
    this.push(null);

    if (hasHandle) {
      handle.destroy();
      session[kState].streams.delete(id);
    } else {
      session[kState].pendingStreams.delete(this);
    }

    // Adjust the write queue size for accounting
    session[kState].writeQueueSize -= state.writeQueueSize;
    state.writeQueueSize = 0;

    // RST code 8 not emitted as an error as its used by clients to signify
    // abort and is already covered by aborted event, also allows more
    // seamless compatibility with http1
    if (err == null && code !== NGHTTP2_NO_ERROR && code !== NGHTTP2_CANCEL)
      err = new ERR_HTTP2_STREAM_ERROR(nameForErrorCode[code] || code);

    this[kSession] = undefined;
    this[kHandle] = undefined;

    // This notifies the session that this stream has been destroyed and
    // gives the session the opportunity to clean itself up. The session
    // will destroy if it has been closed and there are no other open or
    // pending streams.
    session[kMaybeDestroy]();
    callback(err);
  }
  // The Http2Stream can be destroyed if it has closed and if the readable
  // side has received the final chunk.
  [kMaybeDestroy](code = NGHTTP2_NO_ERROR) {
    if (code !== NGHTTP2_NO_ERROR) {
      this.destroy();
      return;
    }

    if (this.writableFinished) {
      if (!this.readable && this.closed) {
        this.destroy();
        return;
      }

      // We've submitted a response from our server session, have not attempted
      // to process any incoming data, and have no trailers. This means we can
      // attempt to gracefully close the session.
      const state = this[kState];
      if (this.headersSent &&
          this[kSession] &&
          this[kSession][kType] === NGHTTP2_SESSION_SERVER &&
          !(state.flags & STREAM_FLAGS_HAS_TRAILERS) &&
          !state.didRead &&
          this.readableFlowing === null) {
        // By using setImmediate we allow pushStreams to make it through
        // before the stream is officially closed. This prevents a bug
        // in most browsers where those pushStreams would be rejected.
        setImmediate(callStreamClose, this);
      }
    }
  }
}

class ServerHttp2Stream extends Http2Stream {
  constructor(session, handle, id, options, headers) {
    super(session, options);
    handle.owner = this;
    this[kInit](id, handle);
    this[kProtocol] = headers[HTTP2_HEADER_SCHEME];
    this[kAuthority] = headers[HTTP2_HEADER_AUTHORITY];
  }

  // True if the remote peer accepts push streams
  get pushAllowed() {
    return !this.destroyed &&
           !this.closed &&
           !this.session.closed &&
           !this.session.destroyed &&
           this[kSession].remoteSettings.enablePush;
  }

  // Create a push stream, call the given callback with the created
  // Http2Stream for the push stream.
  pushStream(headers, options, callback) {
    if (!this.pushAllowed)
      throw new ERR_HTTP2_PUSH_DISABLED();
    if (this[kID] % 2 === 0)
      throw new ERR_HTTP2_NESTED_PUSH();

    const session = this[kSession];

    debugStreamObj(this, 'initiating push stream');

    this[kUpdateTimer]();

    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    if (typeof callback !== 'function')
      throw new ERR_INVALID_CALLBACK(callback);

    assertIsObject(options, 'options');
    options = { ...options };
    options.endStream = !!options.endStream;

    assertIsObject(headers, 'headers');
    headers = ObjectAssign(ObjectCreate(null), headers);

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
    options.readable = false;

    const headersList = mapToHeaders(headers);

    const streamOptions = options.endStream ? STREAM_OPTION_EMPTY_PAYLOAD : 0;

    const ret = this[kHandle].pushPromise(headersList, streamOptions);
    let err;
    if (typeof ret === 'number') {
      switch (ret) {
        case NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE:
          err = new ERR_HTTP2_OUT_OF_STREAMS();
          break;
        case NGHTTP2_ERR_STREAM_CLOSED:
          err = new ERR_HTTP2_INVALID_STREAM();
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
      throw new ERR_HTTP2_INVALID_STREAM();
    if (this.headersSent)
      throw new ERR_HTTP2_HEADERS_SENT();

    const state = this[kState];

    assertIsObject(options, 'options');
    options = { ...options };

    debugStreamObj(this, 'initiating response');
    this[kUpdateTimer]();

    options.endStream = !!options.endStream;

    let streamOptions = 0;
    if (options.endStream)
      streamOptions |= STREAM_OPTION_EMPTY_PAYLOAD;

    if (options.waitForTrailers) {
      streamOptions |= STREAM_OPTION_GET_TRAILERS;
      state.flags |= STREAM_FLAGS_HAS_TRAILERS;
    }

    headers = processHeaders(headers);
    const headersList = mapToHeaders(headers, assertValidPseudoHeaderResponse);
    this[kSentHeaders] = headers;

    state.flags |= STREAM_FLAGS_HEADERS_SENT;

    // Close the writable side if the endStream option is set or status
    // is one of known codes with no payload, or it's a head request
    const statusCode = headers[HTTP2_HEADER_STATUS] | 0;
    if (!!options.endStream ||
        statusCode === HTTP_STATUS_NO_CONTENT ||
        statusCode === HTTP_STATUS_RESET_CONTENT ||
        statusCode === HTTP_STATUS_NOT_MODIFIED ||
        this.headRequest === true) {
      options.endStream = true;
      this.end();
    }

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
      throw new ERR_HTTP2_INVALID_STREAM();
    if (this.headersSent)
      throw new ERR_HTTP2_HEADERS_SENT();

    const session = this[kSession];

    assertIsObject(options, 'options');
    options = { ...options };

    if (options.offset !== undefined && typeof options.offset !== 'number')
      throw new ERR_INVALID_OPT_VALUE('offset', options.offset);

    if (options.length !== undefined && typeof options.length !== 'number')
      throw new ERR_INVALID_OPT_VALUE('length', options.length);

    if (options.statCheck !== undefined &&
        typeof options.statCheck !== 'function') {
      throw new ERR_INVALID_OPT_VALUE('statCheck', options.statCheck);
    }

    let streamOptions = 0;
    if (options.waitForTrailers) {
      streamOptions |= STREAM_OPTION_GET_TRAILERS;
      this[kState].flags |= STREAM_FLAGS_HAS_TRAILERS;
    }

    if (fd instanceof fsPromisesInternal.FileHandle)
      fd = fd.fd;
    else if (typeof fd !== 'number')
      throw new ERR_INVALID_ARG_TYPE('fd', ['number', 'FileHandle'], fd);

    debugStreamObj(this, 'initiating response from fd');
    this[kUpdateTimer]();
    this.ownsFd = false;

    headers = processHeaders(headers);
    const statusCode = headers[HTTP2_HEADER_STATUS] |= 0;
    // Payload/DATA frames are not permitted in these cases
    if (statusCode === HTTP_STATUS_NO_CONTENT ||
        statusCode === HTTP_STATUS_RESET_CONTENT ||
        statusCode === HTTP_STATUS_NOT_MODIFIED ||
        this.headRequest) {
      throw new ERR_HTTP2_PAYLOAD_FORBIDDEN(statusCode);
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
      throw new ERR_HTTP2_INVALID_STREAM();
    if (this.headersSent)
      throw new ERR_HTTP2_HEADERS_SENT();

    assertIsObject(options, 'options');
    options = { ...options };

    if (options.offset !== undefined && typeof options.offset !== 'number')
      throw new ERR_INVALID_OPT_VALUE('offset', options.offset);

    if (options.length !== undefined && typeof options.length !== 'number')
      throw new ERR_INVALID_OPT_VALUE('length', options.length);

    if (options.statCheck !== undefined &&
        typeof options.statCheck !== 'function') {
      throw new ERR_INVALID_OPT_VALUE('statCheck', options.statCheck);
    }

    let streamOptions = 0;
    if (options.waitForTrailers) {
      streamOptions |= STREAM_OPTION_GET_TRAILERS;
      this[kState].flags |= STREAM_FLAGS_HAS_TRAILERS;
    }

    const session = this[kSession];
    debugStreamObj(this, 'initiating response from file');
    this[kUpdateTimer]();
    this.ownsFd = true;

    headers = processHeaders(headers);
    const statusCode = headers[HTTP2_HEADER_STATUS] |= 0;
    // Payload/DATA frames are not permitted in these cases
    if (statusCode === HTTP_STATUS_NO_CONTENT ||
        statusCode === HTTP_STATUS_RESET_CONTENT ||
        statusCode === HTTP_STATUS_NOT_MODIFIED ||
        this.headRequest) {
      throw new ERR_HTTP2_PAYLOAD_FORBIDDEN(statusCode);
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
      throw new ERR_HTTP2_INVALID_STREAM();
    if (this.headersSent)
      throw new ERR_HTTP2_HEADERS_AFTER_RESPOND();

    assertIsObject(headers, 'headers');
    headers = ObjectAssign(ObjectCreate(null), headers);

    debugStreamObj(this, 'sending additional headers');

    if (headers[HTTP2_HEADER_STATUS] != null) {
      const statusCode = headers[HTTP2_HEADER_STATUS] |= 0;
      if (statusCode === HTTP_STATUS_SWITCHING_PROTOCOLS)
        throw new ERR_HTTP2_STATUS_101();
      if (statusCode < 100 || statusCode >= 200) {
        throw new ERR_HTTP2_INVALID_INFO_STATUS(headers[HTTP2_HEADER_STATUS]);
      }
    }

    this[kUpdateTimer]();

    const headersList = mapToHeaders(headers, assertValidPseudoHeaderResponse);
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

ObjectDefineProperty(Http2Stream.prototype, 'setTimeout', {
  configurable: true,
  enumerable: true,
  writable: true,
  value: setStreamTimeout
});

// Also keep track of listeners for the Http2Stream instances, as some events
// are emitted on those objects.
function streamListenerAdded(name) {
  const session = this[kSession];
  if (!session) return;
  switch (name) {
    case 'priority':
      session[kNativeFields][kSessionPriorityListenerCount]++;
      break;
    case 'frameError':
      session[kNativeFields][kSessionFrameErrorListenerCount]++;
      break;
  }
}

function streamListenerRemoved(name) {
  const session = this[kSession];
  if (!session) return;
  switch (name) {
    case 'priority':
      session[kNativeFields][kSessionPriorityListenerCount]--;
      break;
    case 'frameError':
      session[kNativeFields][kSessionFrameErrorListenerCount]--;
      break;
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

function streamOnResume() {
  if (!this.destroyed)
    this[kHandle].readStart();
}

function streamOnPause() {
  if (!this.destroyed && !this.pending)
    this[kHandle].readStop();
}

function afterShutdown(status) {
  const stream = this.handle[kOwner];
  if (stream) {
    stream.on('finish', () => {
      stream[kMaybeDestroy]();
    });
  }
  // Currently this status value is unused
  this.callback();
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
    this.destroy(new ERR_HTTP2_INVALID_STREAM());
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
    const isDirectory = stat.isDirectory();
    if (options.offset !== undefined || options.offset > 0 ||
        options.length !== undefined || options.length >= 0 ||
        isDirectory) {
      const err = isDirectory ?
        new ERR_HTTP2_SEND_FILE() : new ERR_HTTP2_SEND_FILE_NOSEEK();
      tryClose(fd);
      if (onError)
        onError(err);
      else
        this.destroy(err);
      return;
    }

    options.offset = -1;
    options.length = -1;
  }

  if (this.destroyed || this.closed) {
    tryClose(fd);
    this.destroy(new ERR_HTTP2_INVALID_STREAM());
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
    tryClose(fd);
    return;
  }

  if (stat.isFile()) {
    statOptions.length =
      statOptions.length < 0 ? stat.size - (+statOptions.offset) :
        MathMin(stat.size - (+statOptions.offset),
                statOptions.length);

    headers[HTTP2_HEADER_CONTENT_LENGTH] = statOptions.length;
  }

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
    return;
  }
  state.fd = fd;

  fs.fstat(fd,
           doSendFileFD.bind(this, session, options, fd,
                             headers, streamOptions));
}

function handleHeaderContinue(headers) {
  if (headers[HTTP2_HEADER_STATUS] === HTTP_STATUS_CONTINUE)
    this.emit('continue');
}

function onFileUnpipe() {
  const stream = this.sink[kOwner];
  if (stream.ownsFd)
    this.source.close().catch(stream.destroy.bind(stream));
  else
    this.source.releaseFD();
}

// This is only called once the pipe has returned back control, so
// it only has to handle errors and End-of-File.
function onPipedFileHandleRead() {
  const err = streamBaseState[kReadBytesOrError];
  if (err < 0 && err !== UV_EOF) {
    this.stream.close(NGHTTP2_INTERNAL_ERROR);
  }
}

function processRespondWithFD(self, fd, headers, offset = 0, length = -1,
                              streamOptions = 0) {
  const state = self[kState];
  state.flags |= STREAM_FLAGS_HEADERS_SENT;

  let headersList;
  try {
    headersList = mapToHeaders(headers, assertValidPseudoHeaderResponse);
  } catch (err) {
    self.destroy(err);
    return;
  }
  self[kSentHeaders] = headers;

  // Close the writable side of the stream, but only as far as the writable
  // stream implementation is concerned.
  self._final = null;
  self.end();

  const ret = self[kHandle].respond(headersList, streamOptions);

  if (ret < 0) {
    self.destroy(new NghttpError(ret));
    return;
  }

  defaultTriggerAsyncIdScope(self[async_id_symbol], startFilePipe,
                             self, fd, offset, length);
}

function startFilePipe(self, fd, offset, length) {
  const handle = new FileHandle(fd, offset, length);
  handle.onread = onPipedFileHandleRead;
  handle.stream = self;

  const pipe = new StreamPipe(handle, self[kHandle]);
  pipe.onunpipe = onFileUnpipe;
  pipe.start();

  // Exact length of the file doesn't matter here, since the
  // stream is closing anyway - just use 1 to signify that
  // a write does exist
  trackWriteState(self, 1);
}

function tryClose(fd) {
  // Try to close the file descriptor. If closing fails, assert because
  // an error really should not happen at this point.
  fs.close(fd, (err) => assert.ifError(err));
}

function trackWriteState(stream, bytes) {
  const session = stream[kSession];
  stream[kState].writeQueueSize += bytes;
  session[kState].writeQueueSize += bytes;
  session[kHandle].chunksSentSinceLastWrite = 0;
}

function finishSendTrailers(stream, headersList) {
  // The stream might be destroyed and in that case
  // there is nothing to do.
  // This can happen because finishSendTrailers is
  // scheduled via setImmediate.
  if (stream.destroyed) {
    return;
  }

  stream[kState].flags &= ~STREAM_FLAGS_HAS_TRAILERS;

  const ret = stream[kHandle].trailers(headersList);
  if (ret < 0)
    stream.destroy(new NghttpError(ret));
  else
    stream[kMaybeDestroy]();
}

function callStreamClose(stream) {
  stream.close();
}

function processHeaders(oldHeaders) {
  assertIsObject(oldHeaders, 'headers');
  const headers = ObjectCreate(null);

  if (oldHeaders !== null && oldHeaders !== undefined) {
    // This loop is here for performance reason. Do not change.
    for (const key in oldHeaders) {
      if (ObjectPrototypeHasOwnProperty(oldHeaders, key)) {
        headers[key] = oldHeaders[key];
      }
    }
    headers[kSensitiveHeaders] = oldHeaders[kSensitiveHeaders];
  }

  const statusCode =
    headers[HTTP2_HEADER_STATUS] =
      headers[HTTP2_HEADER_STATUS] | 0 || HTTP_STATUS_OK;

  if (headers[HTTP2_HEADER_DATE] === null ||
      headers[HTTP2_HEADER_DATE] === undefined)
    headers[HTTP2_HEADER_DATE] = utcDate();

  // This is intentionally stricter than the HTTP/1 implementation, which
  // allows values between 100 and 999 (inclusive) in order to allow for
  // backwards compatibility with non-spec compliant code. With HTTP/2,
  // we have the opportunity to start fresh with stricter spec compliance.
  // This will have an impact on the compatibility layer for anyone using
  // non-standard, non-compliant status codes.
  if (statusCode < 200 || statusCode > 599)
    throw new ERR_HTTP2_STATUS_INVALID(headers[HTTP2_HEADER_STATUS]);

  const neverIndex = headers[kSensitiveHeaders];
  if (neverIndex !== undefined && !ArrayIsArray(neverIndex))
    throw new ERR_INVALID_OPT_VALUE('headers[http2.neverIndex]', neverIndex);

  return headers;
}

function closeStream(stream, code, rstStreamStatus = kSubmitRstStream) {
  const state = stream[kState];
  state.flags |= STREAM_FLAGS_CLOSED;
  state.rstCode = code;

  // Clear timeout and remove timeout listeners
  stream.setTimeout(0);
  stream.removeAllListeners('timeout');

  const { ending } = stream._writableState;

  if (!ending) {
    // If the writable side of the Http2Stream is still open, emit the
    // 'aborted' event and set the aborted flag.
    if (!stream.aborted) {
      state.flags |= STREAM_FLAGS_ABORTED;
      stream.emit('aborted');
    }

    // Close the writable side.
    stream.end();
  }

  if (rstStreamStatus !== kNoRstStream) {
    const finishFn = finishCloseStream.bind(stream, code);
    if (!ending || stream.writableFinished || code !== NGHTTP2_NO_ERROR ||
        rstStreamStatus === kForceRstStream)
      finishFn();
    else
      stream.once('finish', finishFn);
  }
}

function finishCloseStream(code) {
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

// Submit an RST-STREAM frame to be sent to the remote peer.
// This will cause the Http2Stream to be closed.
function submitRstStream(code) {
  if (this[kHandle] !== undefined) {
    this[kHandle].rstStream(code);
  }
}

// Called when the Http2Stream has finished sending data and is ready for
// trailers to be sent. This will only be called if the { hasOptions: true }
// option is set.
function onStreamTrailers() {
  const stream = this[kOwner];
  stream[kState].trailersReady = true;
  if (stream.destroyed || stream.closed)
    return;
  if (!stream.emit('wantTrailers')) {
    // There are no listeners, send empty trailing HEADERS frame and close.
    stream.sendTrailers({});
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
  if (!stream || stream.destroyed)
    return false;

  debugStreamObj(
    stream, 'closed with code %d, closed %s, readable %s',
    code, stream.closed, stream.readable
  );

  if (!stream.closed)
    closeStream(stream, code, kNoRstStream);

  stream[kState].fd = -1;
  // Defer destroy we actually emit end.
  if (!stream.readable || code !== NGHTTP2_NO_ERROR) {
    // If errored or ended, we can destroy immediately.
    stream.destroy();
  } else {
    // Wait for end to destroy.
    stream.on('end', stream[kMaybeDestroy]);
    // Push a null so the stream can end whenever the client consumes
    // it completely.
    stream.push(null);

    // If the user hasn't tried to consume the stream (and this is a server
    // session) then just dump the incoming data so that the stream can
    // be destroyed.
    if (stream[kSession][kType] === NGHTTP2_SESSION_SERVER &&
        !stream[kState].didRead &&
        stream.readableFlowing === null)
      stream.resume();
    else
      stream.read(0);
  }
  return true;
}

module.exports = {
  debugStream,
  kAuthority,
  kInit,
  kNativeFields,
  kType,
  kProtocol,
  kSentHeaders,
  onStreamClose,
  onStreamTrailers,
  ClientHttp2Stream,
  Http2Stream,
  ServerHttp2Stream,
  STREAM_FLAGS_HAS_TRAILERS,
  STREAM_FLAGS_HEAD_REQUEST,
};
