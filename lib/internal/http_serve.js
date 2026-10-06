'use strict';

const {
  Error: PrimordialError,
  Promise,
  StringPrototypeToLowerCase,
  Symbol,
  Uint8Array,
} = primordials;

const net = require('net');
const tls = require('tls');
const { once } = require('events');
const { setImmediate } = require('timers');
const { ReadableStream } = require('internal/webstreams/readablestream');
const { Response } = require('internal/deps/undici/undici');
const {
  NodeResponse,
  createServeRequest,
  getServeMetadata,
  abortServeRequest,
  getResponseState,
  HeadersList,
  isLazyResponseBody,
  hasMaterializedStream,
} = require('internal/http_serve_classes');

const {
  HTTPParser,
  isLenient,
  prepareError,
  allMethods,
} = require('_http_common');
const { STATUS_CODES } = require('_http_server');
const { ConnectionsList } = internalBinding('http_parser');
const FreeList = require('internal/freelist');

const {
  utcDate,
} = require('internal/http');

const { validateFunction, validateObject } = require('internal/validators');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

const dc = require('diagnostics_channel');
const onRequestStartChannel = dc.channel('http.server.request.start');

// Symbols for private properties on server object
const kHandler = Symbol('kHandler');
const kOnError = Symbol('kOnError');
const kSignal = Symbol('kSignal');
const kConnections = Symbol('kConnections');

// Symbols for private properties on sockets
const kConnMeta = Symbol('kConnMeta');
const kInFlight = Symbol('kInFlight');

// Parser callback indexes
const kOnHeaders = HTTPParser.kOnHeaders | 0;
const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;
const kOnBody = HTTPParser.kOnBody | 0;
const kOnMessageComplete = HTTPParser.kOnMessageComplete | 0;
const kOnExecute = HTTPParser.kOnExecute | 0;
const kOnTimeout = HTTPParser.kOnTimeout | 0;
const kLenientAll = HTTPParser.kLenientAll | 0;
const kLenientNone = HTTPParser.kLenientNone | 0;

// serve() installs different callbacks from the regular HTTP client and server,
// so its parsers must not share their callback pool.
const serveParsers = new FreeList('serveParsers', 1000, () => new HTTPParser());

// Async resource for parser initialization
class HTTPServerAsyncResource {
  constructor(type, socket) {
    this.type = type;
    this.socket = socket;
  }
}

/**
 * Build the request HeadersList directly from the parser output in a single
 * pass, detecting Host, Content-Length and Transfer-Encoding along the way.
 * Names and values were already validated by llhttp, so the WebIDL-level
 * re-validation done by the public Headers API is skipped on purpose.
 * @param {string[]} rawHeaders - Raw headers array (key-value pairs)
 * @returns {{headersList: HeadersList, host: string|undefined,
 *            hasBody: boolean, contentLength: number|null}}
 */
function scanHeaders(rawHeaders) {
  const headersList = new HeadersList();
  let host;
  let hasBody = false;
  let contentLength = null;
  for (let i = 0; i < rawHeaders.length; i += 2) {
    const name = StringPrototypeToLowerCase(rawHeaders[i]);
    const value = rawHeaders[i + 1];
    if (name === 'host') {
      host ??= value;
    } else if (name === 'content-length') {
      hasBody = value !== '0';
      contentLength = hasBody ? +value : 0;
    } else if (name === 'transfer-encoding') {
      hasBody = true;
      contentLength = null;
    }
    headersList.append(name, value, true);
  }
  return { headersList, host, hasBody, contentLength };
}

/**
 * Get (and lazily cache) the address metadata for a connection. The object is
 * shared by every request on a keep-alive connection and stays valid after
 * the socket is torn down.
 * @param {net.Socket|tls.TLSSocket} socket
 * @returns {{remoteAddress: string, remotePort: number, localAddress: string, localPort: number, encrypted: boolean}}
 */
function getConnectionMetadata(socket) {
  return socket[kConnMeta] ??= {
    remoteAddress: socket.remoteAddress,
    remotePort: socket.remotePort,
    localAddress: socket.localAddress,
    localPort: socket.localPort,
    encrypted: !!socket.encrypted,
  };
}

/**
 * Get metadata for a Request object created by serve().
 * @param {Request} request
 * @returns {{remoteAddress: string, remotePort: number, localAddress: string, localPort: number, encrypted: boolean}}
 */
function getRemoteMetadata(request) {
  const metadata = getServeMetadata(request);
  if (metadata === undefined) {
    throw new ERR_INVALID_ARG_TYPE('request', 'Request from serve() handler', request);
  }
  return metadata;
}

/**
 * Create a ReadableStream that bridges parser body events.
 * @param {HTTPParser} parser
 * @param {net.Socket} socket
 * @returns {{stream: ReadableStream, setCallbacks: Function}}
 */
function createRequestBodyStream(parser, socket) {
  let controller;
  let onBody;
  let onComplete;
  let closed = false;
  let complete = false;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      // Resume parser when consumer is ready for more data.
      if (!closed && socket.parser === parser) {
        parser.resume();
      }
    },
    cancel() {
      closed = true;
      if (socket.parser === parser) parser.resume();
    },
  });

  function setCallbacks() {
    onBody = (chunk) => {
      if (closed) return;
      controller.enqueue(new Uint8Array(chunk));
      // Backpressure: pause parser if the queue is full
      if (controller.desiredSize <= 0) {
        parser.pause();
      }
    };

    onComplete = () => {
      complete = true;
      if (closed) return;
      closed = true;
      controller.close();
    };

    parser[kOnBody] = onBody;
    parser[kOnMessageComplete] = onComplete;
  }

  function discard(onDiscarded) {
    if (complete) return false;
    closed = true;
    parser[kOnBody] = noop;
    parser[kOnMessageComplete] = () => {
      complete = true;
      controller.close();
      onDiscarded();
    };
    return true;
  }

  function abort(reason) {
    if (closed) return;
    closed = true;
    controller.error(reason);
  }

  function isComplete() {
    return complete;
  }

  return { stream, setCallbacks, discard, abort, isComplete };
}

/**
 * Create a Request object from parser output.
 * @param {string[]} rawHeaders - Raw headers array
 * @param {number} method - HTTP method index
 * @param {string} url - Request URL path
 * @param {net.Socket} socket
 * @param {HTTPParser} parser
 * @returns {Request}
 */
function createRequest(rawHeaders, method, url, socket, parser) {
  // Build the headers list and the full URL.
  const scanned = scanHeaders(rawHeaders);
  const host = scanned.host || `${socket.localAddress}:${socket.localPort}`;
  const protocol = socket.encrypted ? 'https:' : 'http:';
  const fullUrl = `${protocol}//${host}${url}`;

  // Determine method name
  const methodName = allMethods[method];

  // Create body stream for methods that can have a body
  let body = null;
  let bodyControl = null;
  if (methodName !== 'GET' && methodName !== 'HEAD' && scanned.hasBody) {
    bodyControl = createRequestBodyStream(parser, socket);
    body = {
      stream: bodyControl.stream,
      source: null,
      length: scanned.contentLength,
    };
  }

  const request = createServeRequest(
    methodName, fullUrl, scanned.headersList, body,
    getConnectionMetadata(socket),
  );

  return { request, bodyControl };
}

/**
 * Write a Response to the socket.
 * @param {Response} response
 * @param {net.Socket} socket
 * @param {boolean} keepAlive
 * @returns {Promise|undefined}
 */
function writeResponse(response, socket, keepAlive, headRequest = false) {
  if (!(response instanceof Response)) {
    return writeForeignResponse(response, socket, keepAlive, headRequest);
  }

  const state = getResponseState(response);
  if (state.status === 0) {
    // Response.error() and filtered responses cannot be serialized.
    throw new ERR_INVALID_STATE('Network error responses cannot be sent');
  }
  const body = state.body ?? null;
  const statusText = state.statusText || STATUS_CODES[state.status] || 'Unknown';
  let head = `HTTP/1.1 ${state.status} ${statusText}\r\n`;

  let hasContentLength = false;
  let hasTransferEncoding = false;
  let hasDate = false;
  let hasConnection = false;

  const headersList = state.headersList;
  for (const { 0: name, 1: entry } of headersList.headersMap) {
    if (name === 'set-cookie') continue; // Written individually below.
    if (name === 'content-length') hasContentLength = true;
    else if (name === 'transfer-encoding') hasTransferEncoding = true;
    else if (name === 'date') hasDate = true;
    else if (name === 'connection') hasConnection = true;
    head += `${name}: ${entry.value}\r\n`;
  }
  // Multiple Set-Cookie headers must each go on their own line; the
  // headersMap entry holds them joined with ', ', which clients misparse.
  const cookies = headersList.cookies;
  if (cookies !== null) {
    for (let i = 0; i < cookies.length; i++) {
      head += `set-cookie: ${cookies[i]}\r\n`;
    }
  }

  // A body whose source bytes are available can be written directly,
  // without touching (or, for lazy bodies, even creating) its stream.
  let source;
  if (body !== null) {
    if (isLazyResponseBody(body) && !hasMaterializedStream(body)) {
      if (body.used) {
        throw new ERR_INVALID_STATE('Response body is unusable');
      }
      source = body.source;
    } else {
      if (response.bodyUsed || body.stream.locked) {
        throw new ERR_INVALID_STATE('Response body is unusable');
      }
      const candidate = body.source;
      if (candidate instanceof Uint8Array) {
        source = candidate;
      } else if (typeof candidate === 'string' && candidate.length <= 8192) {
        // The stream already holds the encoded bytes; writing the string
        // re-encodes it, which only beats the stream loop for small bodies.
        source = candidate;
      }
    }
  }

  if (!hasContentLength && !hasTransferEncoding &&
      body !== null && body.length !== null) {
    head += `Content-Length: ${body.length}\r\n`;
    hasContentLength = true;
  }

  const chunked = !headRequest && body !== null &&
    !hasContentLength && !hasTransferEncoding;
  if (chunked) {
    head += 'Transfer-Encoding: chunked\r\n';
  }

  if (!hasConnection) {
    head += keepAlive ? 'Connection: keep-alive\r\n' : 'Connection: close\r\n';
  }

  if (!hasDate) {
    head += `Date: ${utcDate()}\r\n`;
  }
  head += '\r\n';

  if (headRequest || body === null || body.length === 0) {
    const headWritten = socket.write(head);
    markResponseBodyUsed(body);
    if (!headWritten) return once(socket, 'drain');
    return;
  }

  // Write the source bytes directly, skipping the Web Streams reader loop.
  if (source !== undefined) {
    socket.cork();
    socket.write(head);
    const bodyWritten = socket.write(source);
    socket.uncork();
    markResponseBodyUsed(body);
    if (!bodyWritten) return once(socket, 'drain');
    return;
  }

  const headWritten = socket.write(head);
  if (!headWritten) {
    return writeStreamingBodyAfterDrain(body.stream, socket, chunked);
  }
  return writeStreamingBody(body.stream, socket, chunked);
}

function markResponseBodyUsed(body) {
  if (body === null) return;
  if (isLazyResponseBody(body) && !hasMaterializedStream(body)) {
    body.used = true;
  } else {
    body.stream.cancel().catch(noop);
  }
}

/**
 * Serialize a fetch-compatible Response-like object that is not an instance
 * of the bundled undici Response (e.g. one produced by a copy of undici
 * shipped inside a framework's node_modules) using only its public API.
 * @param {object} response
 * @param {net.Socket} socket
 * @param {boolean} keepAlive
 * @param {boolean} headRequest
 * @returns {Promise|undefined}
 */
function writeForeignResponse(response, socket, keepAlive, headRequest) {
  const status = response.status;
  if (status === 0) {
    throw new ERR_INVALID_STATE('Network error responses cannot be sent');
  }
  const statusText = response.statusText || STATUS_CODES[status] || 'Unknown';
  let head = `HTTP/1.1 ${status} ${statusText}\r\n`;

  let hasContentLength = false;
  let hasTransferEncoding = false;
  let hasDate = false;
  let hasConnection = false;

  // Public Headers iteration is sorted, lowercased and yields set-cookie
  // entries individually.
  for (const { 0: name, 1: value } of response.headers) {
    if (name === 'content-length') hasContentLength = true;
    else if (name === 'transfer-encoding') hasTransferEncoding = true;
    else if (name === 'date') hasDate = true;
    else if (name === 'connection') hasConnection = true;
    head += `${name}: ${value}\r\n`;
  }

  const body = response.body ?? null;
  if (body !== null && (response.bodyUsed || body.locked)) {
    throw new ERR_INVALID_STATE('Response body is unusable');
  }

  const noBody = headRequest || body === null ||
    status === 204 || status === 304;
  const chunked = !noBody && !hasContentLength && !hasTransferEncoding;
  if (chunked) {
    head += 'Transfer-Encoding: chunked\r\n';
  }
  if (!hasConnection) {
    head += keepAlive ? 'Connection: keep-alive\r\n' : 'Connection: close\r\n';
  }
  if (!hasDate) {
    head += `Date: ${utcDate()}\r\n`;
  }
  head += '\r\n';

  if (noBody) {
    const headWritten = socket.write(head);
    if (body !== null) body.cancel().catch(noop);
    if (!headWritten) return once(socket, 'drain');
    return;
  }

  const headWritten = socket.write(head);
  if (!headWritten) {
    return writeStreamingBodyAfterDrain(body, socket, chunked);
  }
  return writeStreamingBody(body, socket, chunked);
}

async function writeStreamingBodyAfterDrain(stream, socket, chunked) {
  await once(socket, 'drain');
  return writeStreamingBody(stream, socket, chunked);
}

async function writeStreamingBody(stream, socket, chunked) {
  const reader = stream.getReader();
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;

      if (chunked) {
        socket.cork();
        socket.write(`${value.length.toString(16)}\r\n`);
        socket.write(value);
        socket.write('\r\n');
        socket.uncork();
      } else {
        socket.write(value);
      }

      if (socket.writableNeedDrain) {
        await once(socket, 'drain');
      }
    }
    if (chunked) {
      socket.write('0\r\n\r\n');
    }
  } finally {
    reader.releaseLock();
  }
}

function noop() {}

function completeRequest(socket, keepAlive, parser, resume = true) {
  // A half-closed connection cannot receive further requests, so finish it
  // even if the client asked for keep-alive.
  if (keepAlive && !socket.destroyed && !socket.readableEnded &&
      socket.parser === parser) {
    if (resume) parser.resume();
  } else if (!socket.destroyed) {
    socket.end();
  }
}

function completeResponse(socket, keepAlive, parser, bodyControl, resume) {
  // The response is fully written: client disconnects are no longer aborts.
  socket[kInFlight] = undefined;
  if (bodyControl && bodyControl.discard(() => {
    completeRequest(socket, keepAlive, parser, false);
  })) {
    if (resume && socket.parser === parser) parser.resume();
    return;
  }
  completeRequest(socket, keepAlive, parser, resume);
}

function validateResponse(response) {
  if (response instanceof Response) return;
  // A fetch-compatible Response from another undici instance (e.g. bundled
  // inside a framework) is accepted and serialized through its public API.
  if (response !== null && typeof response === 'object' &&
      typeof response.status === 'number' &&
      typeof response.headers?.getSetCookie === 'function') {
    return;
  }
  throw new ERR_INVALID_ARG_TYPE('handler return value', 'Response', response);
}

async function handleHandlerError(
  server, socket, request, keepAlive, parser, bodyControl, error, headersSent,
) {
  if (!headersSent && !socket.destroyed) {
    const onError = server[kOnError];
    let errorResponse;

    if (onError) {
      try {
        errorResponse = await onError(error, request);
      } catch {
        errorResponse = new NodeResponse('Internal Server Error', { status: 500 });
      }
    } else {
      errorResponse = new NodeResponse('Internal Server Error', { status: 500 });
    }

    try {
      await writeResponse(errorResponse, socket, false, request.method === 'HEAD');
    } catch {
      // Ignore write errors when sending error response.
    }
  }

  server.emit('error', error);
  completeResponse(socket, keepAlive, parser, bodyControl, true);
}

async function finishAsyncResponse(
  server, socket, request, keepAlive, parser, bodyControl, response,
) {
  let headersSent = false;
  try {
    response = await response;
    validateResponse(response);
    headersSent = true;
    await writeResponse(response, socket, keepAlive, request.method === 'HEAD');
    completeResponse(socket, keepAlive, parser, bodyControl, true);
  } catch (error) {
    await handleHandlerError(
      server, socket, request, keepAlive, parser, bodyControl,
      error, headersSent,
    );
  }
}

async function finishStreamingResponse(
  server, socket, request, keepAlive, parser, bodyControl, writing,
) {
  try {
    await writing;
    completeResponse(socket, keepAlive, parser, bodyControl, true);
  } catch (error) {
    await handleHandlerError(
      server, socket, request, keepAlive, parser, bodyControl, error, true,
    );
  }
}

/**
 * Invoke the handler and manage the response.
 * @param {net.Server|tls.Server} server
 * @param {net.Socket} socket
 * @param {Request} request
 * @param {boolean} keepAlive
 * @param {HTTPParser} parser
 * @param {object|null} bodyControl
 * @returns {boolean} Whether response completion is asynchronous
 */
function invokeHandler(server, socket, request, keepAlive, parser, bodyControl) {
  if (bodyControl) {
    bodyControl.setCallbacks();
  } else {
    parser[kOnBody] = noop;
    parser[kOnMessageComplete] = noop;
  }

  try {
    const response = server[kHandler](request);
    if (response instanceof Promise) {
      finishAsyncResponse(
        server, socket, request, keepAlive, parser, bodyControl, response,
      );
      return true;
    }

    validateResponse(response);
    const writing = writeResponse(
      response, socket, keepAlive, request.method === 'HEAD',
    );
    if (writing instanceof Promise) {
      finishStreamingResponse(
        server, socket, request, keepAlive, parser, bodyControl, writing,
      );
      return true;
    }

    completeResponse(socket, keepAlive, parser, bodyControl, false);
    return false;
  } catch (error) {
    handleHandlerError(
      server, socket, request, keepAlive, parser, bodyControl, error, false,
    );
    return true;
  }
}

function closeParser(parser) {
  parser.close();
}

function freeServeParser(parser, socket) {
  if (socket.parser !== parser) return;

  if (parser._consumed) parser.unconsume();
  parser.remove();
  parser[kOnHeaders] = null;
  parser[kOnHeadersComplete] = null;
  parser[kOnBody] = null;
  parser[kOnMessageComplete] = null;
  parser[kOnExecute] = null;
  parser[kOnTimeout] = null;
  parser.socket = null;
  socket.parser = null;

  if (serveParsers.free(parser)) {
    parser.free();
  } else {
    setImmediate(closeParser, parser);
  }
}

/**
 * Handle a new connection.
 * @param {net.Server|tls.Server} server
 * @param {net.Socket|tls.TLSSocket} socket
 */
function handleConnection(server, socket) {
  // Allocate and initialize a parser from the serve()-specific pool.
  const parser = serveParsers.alloc();

  const lenient = isLenient();

  parser.initialize(
    HTTPParser.REQUEST,
    new HTTPServerAsyncResource('HTTPINCOMINGMESSAGE', socket),
    0, // maxHeaderSize (0 = use default)
    lenient ? kLenientAll : kLenientNone,
    server[kConnections],
  );

  parser.socket = socket;
  parser._consumed = false;
  socket.parser = parser;

  // Track parser state
  parser._headers = [];
  parser._url = '';

  // Handle fragmented headers
  parser[kOnHeaders] = function onHeaders(headers, url) {
    this._headers.push(...headers);
    this._url += url;
  };

  // Main callback when headers are complete
  parser[kOnHeadersComplete] = function onHeadersComplete(
    versionMajor, versionMinor, headers, method,
    url, statusCode, statusMessage, upgrade, shouldKeepAlive,
  ) {
    // Use accumulated headers if fragmented
    if (headers === undefined) {
      headers = this._headers;
      this._headers = [];
    }
    if (url === undefined) {
      url = this._url;
      this._url = '';
    }

    // Handle upgrade requests (WebSocket, etc.)
    if (upgrade) {
      server.emit('upgrade', { headers, method: allMethods[method], url }, socket);
      return 1; // Skip body parsing
    }

    // Create Request and invoke handler.
    const { request, bodyControl } = createRequest(headers, method, url, socket, parser);

    // Track the in-flight request so a client disconnect before the response
    // completes aborts request.signal and errors the body stream.
    socket[kInFlight] = { request, bodyControl };

    // Publish to diagnostics channel
    if (onRequestStartChannel.hasSubscribers) {
      onRequestStartChannel.publish({
        request,
        socket,
        server,
      });
    }

    // Request bodies need the parser paused before the handler starts so a
    // ReadableStream pull can resume it. Bodyless synchronous responses avoid
    // the pause/resume cycle entirely.
    if (bodyControl) parser.pause();
    const pending = invokeHandler(
      server, socket, request, shouldKeepAlive, parser, bodyControl,
    );
    if (pending) {
      if (!bodyControl) parser.pause();
    } else if (bodyControl) {
      parser.resume();
    }

    return 0;
  };

  // Parser execution callback (for consumed sockets)
  parser[kOnExecute] = function onParserExecute(ret) {
    socket._unrefTimer?.();
    if (ret instanceof PrimordialError) {
      prepareError(ret, parser, undefined);
      socketOnError(socket, server, ret);
    }
  };

  // Parser timeout callback
  parser[kOnTimeout] = function onParserTimeout() {
    const serverTimeout = server.emit('timeout', socket);
    if (!serverTimeout) {
      socket.destroy();
    }
  };

  // Consume the socket for zero-copy parsing
  if (socket._handle?.isStreamBase && !socket._handle._consumed) {
    parser._consumed = true;
    socket._handle._consumed = true;
    parser.consume(socket._handle);
  }

  // Socket event handlers
  socket.on('error', (err) => socketOnError(socket, server, err));
  socket.on('end', () => {
    // The connection allows half-open so clients may send FIN and still
    // receive their response, matching http.Server.
    const inFlight = socket[kInFlight];
    if (inFlight !== undefined) {
      // The client stopped sending: a request body that is still incomplete
      // can never complete, so the request is aborted. A complete request
      // just keeps waiting for its response.
      if (inFlight.bodyControl !== null && !inFlight.bodyControl.isComplete()) {
        socket[kInFlight] = undefined;
        abortServeRequest(inFlight.request);
        inFlight.bodyControl.abort(inFlight.request.signal.reason);
      }
    } else if (!socket.destroyed) {
      socket.end();
    }
  });
  socket.on('close', () => {
    freeServeParser(parser, socket);
    const inFlight = socket[kInFlight];
    if (inFlight !== undefined) {
      socket[kInFlight] = undefined;
      abortServeRequest(inFlight.request);
      inFlight.bodyControl?.abort(inFlight.request.signal.reason);
    }
  });
}

/**
 * Handle socket errors.
 * @param {net.Socket} socket
 * @param {net.Server} server
 * @param {Error} err
 */
function socketOnError(socket, server, err) {
  if (!server.emit('clientError', err, socket)) {
    // Default error handling
    if (socket.writable && !socket._httpMessage) {
      socket.write('HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n');
    }
    socket.destroy(err);
  }
}

/**
 * Create an HTTP server that handles requests using the Fetch API model.
 * @param {object} options - Server options
 * @param {object} [options.tls] - TLS options (key, cert, etc.) for HTTPS
 * @param {AbortSignal} [options.signal] - Signal for graceful shutdown
 * @param {Function} [options.onError] - Error handler (error, request) => Response
 * @param {Function} handler - Request handler (request) => Response
 * @returns {net.Server|tls.Server}
 */
function serve(options, handler) {
  // Validate arguments
  if (typeof options === 'function') {
    handler = options;
    options = {};
  }

  validateObject(options, 'options');
  validateFunction(handler, 'handler');

  // Create base server (net or tls)
  let baseServer;
  if (options.tls) {
    validateObject(options.tls, 'options.tls');
    baseServer = tls.createServer({ noDelay: true, ...options.tls });
  } else {
    baseServer = net.createServer({ allowHalfOpen: true, noDelay: true });
  }

  // Attach handler and options to server
  baseServer[kHandler] = handler;
  baseServer[kOnError] = options.onError;
  baseServer[kSignal] = options.signal;

  // Initialize connections tracking
  baseServer[kConnections] = new ConnectionsList();

  // Set up connection listener
  const connectionEvent = options.tls ? 'secureConnection' : 'connection';
  baseServer.on(connectionEvent, (socket) => handleConnection(baseServer, socket));

  // Handle abort signal for graceful shutdown
  if (options.signal) {
    if (options.signal.aborted) {
      process.nextTick(() => baseServer.close());
    } else {
      options.signal.addEventListener('abort', () => {
        baseServer.close();
      }, { once: true });
    }
  }

  return baseServer;
}

module.exports = {
  serve,
  getRemoteMetadata,
};
