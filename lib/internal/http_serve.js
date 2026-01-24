'use strict';

const {
  Error: PrimordialError,
  Promise,
  SafeWeakMap,
  Symbol,
  Uint8Array,
} = primordials;

const net = require('net');
const tls = require('tls');
const { once } = require('events');
const { ReadableStream } = require('internal/webstreams/readablestream');
const {
  Headers,
  Request,
  Response,
} = require('internal/deps/undici/undici');

const {
  parsers,
  freeParser,
  HTTPParser,
  isLenient,
  prepareError,
  allMethods,
} = require('_http_common');
const { ConnectionsList } = internalBinding('http_parser');

const {
  utcDate,
} = require('internal/http');

const { validateFunction, validateObject } = require('internal/validators');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const dc = require('diagnostics_channel');
const onRequestStartChannel = dc.channel('http.server.request.start');

// Symbols for private properties on server object
const kHandler = Symbol('kHandler');
const kOnError = Symbol('kOnError');
const kSignal = Symbol('kSignal');
const kConnections = Symbol('kConnections');

// Parser callback indexes
const kOnHeaders = HTTPParser.kOnHeaders | 0;
const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;
const kOnBody = HTTPParser.kOnBody | 0;
const kOnMessageComplete = HTTPParser.kOnMessageComplete | 0;
const kOnExecute = HTTPParser.kOnExecute | 0;
const kOnTimeout = HTTPParser.kOnTimeout | 0;
const kLenientAll = HTTPParser.kLenientAll | 0;
const kLenientNone = HTTPParser.kLenientNone | 0;

// WeakMap for storing request metadata (O(1) lookup, no memory leak)
const requestMetadata = new SafeWeakMap();

// HTTP status codes
const STATUS_CODES = {
  100: 'Continue',
  101: 'Switching Protocols',
  200: 'OK',
  201: 'Created',
  202: 'Accepted',
  204: 'No Content',
  301: 'Moved Permanently',
  302: 'Found',
  304: 'Not Modified',
  400: 'Bad Request',
  401: 'Unauthorized',
  403: 'Forbidden',
  404: 'Not Found',
  405: 'Method Not Allowed',
  408: 'Request Timeout',
  500: 'Internal Server Error',
  501: 'Not Implemented',
  502: 'Bad Gateway',
  503: 'Service Unavailable',
};

// Async resource for parser initialization
class HTTPServerAsyncResource {
  constructor(type, socket) {
    this.type = type;
    this.socket = socket;
  }
}

/**
 * Get the Host header value from raw headers array.
 * @param {string[]} headers - Raw headers array (key-value pairs)
 * @returns {string|undefined}
 */
function getHostHeader(headers) {
  for (let i = 0; i < headers.length; i += 2) {
    if (headers[i].toLowerCase() === 'host') {
      return headers[i + 1];
    }
  }
  return undefined;
}

/**
 * Convert raw headers array to Headers object.
 * @param {string[]} rawHeaders - Raw headers array (key-value pairs)
 * @returns {Headers}
 */
function convertHeaders(rawHeaders) {
  const headersObj = new Headers();
  for (let i = 0; i < rawHeaders.length; i += 2) {
    headersObj.append(rawHeaders[i], rawHeaders[i + 1]);
  }
  return headersObj;
}

/**
 * Store metadata for a Request object.
 * @param {Request} request
 * @param {net.Socket|tls.TLSSocket} socket
 */
function setRequestMetadata(request, socket) {
  requestMetadata.set(request, {
    remoteAddress: socket.remoteAddress,
    remotePort: socket.remotePort,
    localAddress: socket.localAddress,
    localPort: socket.localPort,
    encrypted: !!socket.encrypted,
  });
}

/**
 * Get metadata for a Request object created by serve().
 * @param {Request} request
 * @returns {{remoteAddress: string, remotePort: number, localAddress: string, localPort: number, encrypted: boolean}}
 */
function getRemoteMetadata(request) {
  const meta = requestMetadata.get(request);
  if (!meta) {
    throw new ERR_INVALID_ARG_TYPE('request', 'Request from serve() handler', request);
  }
  return meta;
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

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      // Resume parser when consumer is ready for more data
      if (!closed) {
        parser.resume();
      }
    },
    cancel() {
      closed = true;
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
      if (closed) return;
      closed = true;
      controller.close();
    };

    parser[kOnBody] = onBody;
    parser[kOnMessageComplete] = onComplete;
  }

  return { stream, setCallbacks };
}

/**
 * Create a Request object from parser output.
 * @param {string[]} headers - Raw headers array
 * @param {number} method - HTTP method index
 * @param {string} url - Request URL path
 * @param {net.Socket} socket
 * @param {HTTPParser} parser
 * @returns {Request}
 */
function createRequest(headers, method, url, socket, parser) {
  // Build full URL
  const host = getHostHeader(headers) || `${socket.localAddress}:${socket.localPort}`;
  const protocol = socket.encrypted ? 'https:' : 'http:';
  const fullUrl = `${protocol}//${host}${url}`;

  // Convert raw headers to Headers object
  const headersObj = convertHeaders(headers);

  // Determine method name
  const methodName = allMethods[method];

  // Create body stream for methods that can have a body
  let body = null;
  let setCallbacks = null;
  if (methodName !== 'GET' && methodName !== 'HEAD') {
    const result = createRequestBodyStream(parser, socket);
    body = result.stream;
    setCallbacks = result.setCallbacks;
  }

  // Create Request object
  const request = new Request(fullUrl, {
    method: methodName,
    headers: headersObj,
    body,
    duplex: body ? 'half' : undefined,
  });

  // Store metadata for getRemoteMetadata()
  setRequestMetadata(request, socket);

  return { request, setCallbacks };
}

/**
 * Write a Response to the socket.
 * @param {Response} response
 * @param {net.Socket} socket
 * @param {boolean} keepAlive
 */
async function writeResponse(response, socket, keepAlive) {
  // Status line
  const statusText = response.statusText || STATUS_CODES[response.status] || 'Unknown';
  socket.write(`HTTP/1.1 ${response.status} ${statusText}\r\n`);

  // Write headers
  let hasContentLength = false;
  let hasDate = false;
  let hasConnection = false;

  for (const { 0: name, 1: value } of response.headers) {
    const lowerName = name.toLowerCase();
    if (lowerName === 'content-length') hasContentLength = true;
    if (lowerName === 'date') hasDate = true;
    if (lowerName === 'connection') hasConnection = true;
    socket.write(`${name}: ${value}\r\n`);
  }

  // Determine if we need chunked encoding
  const chunked = !hasContentLength && response.body;

  if (chunked) {
    socket.write('Transfer-Encoding: chunked\r\n');
  }

  if (!hasConnection) {
    socket.write(keepAlive ? 'Connection: keep-alive\r\n' : 'Connection: close\r\n');
  }

  if (!hasDate) {
    socket.write(`Date: ${utcDate()}\r\n`);
  }

  // End headers
  socket.write('\r\n');

  // Write body if present
  if (response.body) {
    const reader = response.body.getReader();
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;

        if (chunked) {
          socket.write(`${value.length.toString(16)}\r\n`);
          socket.write(value);
          socket.write('\r\n');
        } else {
          socket.write(value);
        }

        // Handle backpressure
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
}

/**
 * Invoke the handler and manage the response.
 * @param {net.Server|tls.Server} server
 * @param {net.Socket} socket
 * @param {Request} request
 * @param {boolean} keepAlive
 * @param {HTTPParser} parser
 * @param {Function|null} setBodyCallbacks
 */
async function invokeHandler(server, socket, request, keepAlive, parser, setBodyCallbacks) {
  let headersSent = false;

  try {
    // Set up body stream callbacks if there's a body
    if (setBodyCallbacks) {
      setBodyCallbacks();
    } else {
      // No body expected - set up callbacks to handle any unexpected body data
      parser[kOnBody] = () => {};
      parser[kOnMessageComplete] = () => {};
    }

    // Call the handler
    let response = server[kHandler](request);
    if (response instanceof Promise) {
      response = await response;
    }

    // Validate response
    if (!(response instanceof Response)) {
      throw new ERR_INVALID_ARG_TYPE('handler return value', 'Response', response);
    }

    // Write response
    headersSent = true;
    await writeResponse(response, socket, keepAlive);

  } catch (error) {
    // Handle errors
    if (!headersSent) {
      const onError = server[kOnError];
      let errorResponse;

      if (onError) {
        try {
          errorResponse = await onError(error, request);
        } catch {
          errorResponse = new Response('Internal Server Error', { status: 500 });
        }
      } else {
        errorResponse = new Response('Internal Server Error', { status: 500 });
      }

      try {
        await writeResponse(errorResponse, socket, false);
      } catch {
        // Ignore write errors when sending error response
      }
    }

    // Emit error on server
    server.emit('error', error);
  }

  // Resume parser for next request or close connection
  if (keepAlive && !socket.destroyed) {
    parser.resume();
  } else if (!socket.destroyed) {
    socket.end();
  }
}

/**
 * Handle a new connection.
 * @param {net.Server|tls.Server} server
 * @param {net.Socket|tls.TLSSocket} socket
 */
function handleConnection(server, socket) {
  // Allocate and initialize parser
  const parser = parsers.alloc();

  const lenient = isLenient();

  parser.initialize(
    HTTPParser.REQUEST,
    new HTTPServerAsyncResource('HTTPINCOMINGMESSAGE', socket),
    0, // maxHeaderSize (0 = use default)
    lenient ? kLenientAll : kLenientNone,
    server[kConnections],
  );

  parser.socket = socket;
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

    // Pause parser immediately - we control when to resume
    parser.pause();

    // Create Request and invoke handler
    const { request, setCallbacks } = createRequest(headers, method, url, socket, parser);

    // Publish to diagnostics channel
    if (onRequestStartChannel.hasSubscribers) {
      onRequestStartChannel.publish({
        request,
        socket,
        server,
      });
    }

    // Invoke handler asynchronously
    invokeHandler(server, socket, request, shouldKeepAlive, parser, setCallbacks);

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
  socket.on('close', () => {
    freeParser(parser, null, socket);
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
    baseServer = tls.createServer(options.tls);
  } else {
    baseServer = net.createServer({ allowHalfOpen: true });
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
