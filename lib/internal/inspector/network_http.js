'use strict';

const {
  ArrayIsArray,
  DateNow,
  ObjectEntries,
  String,
  Symbol,
} = primordials;

const {
  kInspectorRequestId,
  kResourceType,
  getMonotonicTime,
  getNextRequestId,
  registerDiagnosticChannels,
  sniffMimeType,
} = require('internal/inspector/network');
const { Network } = require('inspector');
const { Buffer } = require('buffer');
const { hasWebSocketHeader } = require('internal/http_websocket_observer');

const kRequestUrl = Symbol('kRequestUrl');
const kIsWebSocketUpgrade = Symbol('kIsWebSocketUpgrade');
const kWebSocketClosed = Symbol('kWebSocketClosed');

// Convert a Headers object (Map<string, number | string | string[]>) to a plain object (Map<string, string>)
const convertHeaderObject = (headers = {}) => {
  // The 'host' header that contains the host and port of the URL.
  let host;
  let charset;
  let mimeType;
  const dict = {};
  for (const { 0: key, 1: value } of ObjectEntries(headers)) {
    const lowerCasedKey = key.toLowerCase();
    if (lowerCasedKey === 'host') {
      host = value;
    }
    if (lowerCasedKey === 'content-type') {
      const result = sniffMimeType(value);
      charset = result.charset;
      mimeType = result.mimeType;
    }
    if (typeof value === 'string') {
      dict[key] = value;
    } else if (ArrayIsArray(value)) {
      if (lowerCasedKey === 'cookie') dict[key] = value.join('; ');
      // ChromeDevTools frontend treats 'set-cookie' as a special case
      // https://github.com/ChromeDevTools/devtools-frontend/blob/4275917f84266ef40613db3c1784a25f902ea74e/front_end/core/sdk/NetworkRequest.ts#L1368
      else if (lowerCasedKey === 'set-cookie') dict[key] = value.join('\n');
      else dict[key] = value.join(', ');
    } else {
      dict[key] = String(value);
    }
  }
  return [dict, host, charset, mimeType];
};

function ensureRequestMetadata(request) {
  if (typeof request[kInspectorRequestId] !== 'string') {
    request[kInspectorRequestId] = getNextRequestId();
  }

  if (typeof request[kRequestUrl] !== 'string') {
    const { 1: host } = convertHeaderObject(request.getHeaders());
    request[kRequestUrl] = `${request.protocol}//${host}${request.path}`;
  }
}

function toWebSocketPayloadData(opcode, payload) {
  if (opcode === 0x1) {
    return payload.toString();
  }
  return payload.toString('base64');
}

function toWebSocketUrl(requestUrl) {
  if (requestUrl.startsWith('https://')) {
    return `wss://${requestUrl.slice('https://'.length)}`;
  }
  if (requestUrl.startsWith('http://')) {
    return `ws://${requestUrl.slice('http://'.length)}`;
  }
  return requestUrl;
}

/**
 * When a client request is created, emit Network.requestWillBeSent event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-requestWillBeSent
 * @param {{ request: import('http').ClientRequest }} event
 */
function onClientRequestCreated({ request }) {
  request[kInspectorRequestId] = getNextRequestId();

  const { 0: headers, 1: host, 2: charset } = convertHeaderObject(request.getHeaders());
  const url = `${request.protocol}//${host}${request.path}`;
  request[kRequestUrl] = url;
  request[kIsWebSocketUpgrade] = hasWebSocketHeader(request.getHeader('upgrade'));

  if (request[kIsWebSocketUpgrade]) {
    return;
  }

  Network.requestWillBeSent({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    wallTime: DateNow(),
    charset,
    request: {
      url,
      method: request.method,
      headers,
    },
  });
}

function onClientRequestUpgrade({ request, response, socket }) {
  ensureRequestMetadata(request);
  request[kIsWebSocketUpgrade] = true;

  const { 0: headers } = convertHeaderObject(request.getHeaders());
  Network.webSocketCreated({
    requestId: request[kInspectorRequestId],
    url: toWebSocketUrl(request[kRequestUrl]),
  });
  Network.webSocketWillSendHandshakeRequest({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    wallTime: DateNow(),
    request: {
      headers,
    },
  });

  const { 0: responseHeaders } = convertHeaderObject(response.headers);
  Network.webSocketHandshakeResponseReceived({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    response: {
      status: response.statusCode,
      statusText: response.statusMessage ?? '',
      headers: responseHeaders,
    },
  });

  socket[kWebSocketClosed] = false;
  socket.once('close', () => {
    if (socket[kWebSocketClosed]) {
      return;
    }
    socket[kWebSocketClosed] = true;
    Network.webSocketClosed({
      requestId: request[kInspectorRequestId],
      timestamp: getMonotonicTime(),
    });
  });
}

function onWebSocketFrameSent({ request, opcode, masked, payload }) {
  ensureRequestMetadata(request);

  Network.webSocketFrameSent({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    response: {
      opcode,
      mask: masked,
      payloadData: toWebSocketPayloadData(opcode, payload),
    },
  });
}

function onWebSocketFrameReceived({ request, opcode, masked, payload }) {
  ensureRequestMetadata(request);

  Network.webSocketFrameReceived({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    response: {
      opcode,
      mask: masked,
      payloadData: toWebSocketPayloadData(opcode, payload),
    },
  });
}

function shouldSkipHttpRequest(request) {
  return typeof request[kInspectorRequestId] !== 'string' ||
    request[kIsWebSocketUpgrade];
}

/**
 * When a client request errors, emit Network.loadingFailed event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-loadingFailed
 * @param {{ request: import('http').ClientRequest, error: any }} event
 */
function onClientRequestError({ request, error }) {
  if (shouldSkipHttpRequest(request)) {
    return;
  }
  Network.loadingFailed({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    type: kResourceType.Other,
    errorText: error.message,
  });
}

/**
 * When a chunk of the request body is being sent, cache it until
 * `getRequestPostData` request.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#method-getRequestPostData
 * @param {{ request: import('http').ClientRequest, chunk: Uint8Array | string, encoding?: string }} event
 */
function onClientRequestBodyChunkSent({ request, chunk, encoding }) {
  if (shouldSkipHttpRequest(request)) {
    return;
  }

  const buffer = typeof chunk === 'string' ? Buffer.from(chunk, encoding) : Buffer.from(chunk);
  Network.dataSent({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    dataLength: buffer.byteLength,
    data: buffer,
  });
}

/**
 * Mark a request body as fully sent.
 * @param {{ request: import('http').ClientRequest }} event
 */
function onClientRequestBodySent({ request }) {
  if (shouldSkipHttpRequest(request)) {
    return;
  }

  Network.dataSent({
    requestId: request[kInspectorRequestId],
    finished: true,
  });
}

/**
 * When a chunk of the response body is received, cache the raw bytes until
 * `getResponseBody` request.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#method-getResponseBody
 * @param {{ request: import('http').ClientRequest, chunk: Uint8Array }} event
 */
function onClientResponseBodyChunkReceived({ request, chunk }) {
  if (shouldSkipHttpRequest(request)) {
    return;
  }

  Network.dataReceived({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    dataLength: chunk.byteLength,
    encodedDataLength: chunk.byteLength,
    data: chunk,
  });
}

/**
 * When response headers are received, emit Network.responseReceived event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-responseReceived
 * @param {{ request: import('http').ClientRequest, error: any }} event
 */
function onClientResponseFinish({ request, response }) {
  if (shouldSkipHttpRequest(request)) {
    return;
  }

  const { 0: headers, 2: charset, 3: mimeType } = convertHeaderObject(response.headers);

  Network.responseReceived({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    type: kResourceType.Other,
    response: {
      url: request[kRequestUrl],
      status: response.statusCode,
      statusText: response.statusMessage ?? '',
      headers,
      mimeType,
      charset,
    },
  });

  // Wait until the response body is consumed by user code.
  response.once('end', () => {
    Network.loadingFinished({
      requestId: request[kInspectorRequestId],
      timestamp: getMonotonicTime(),
    });
  });
}

module.exports = registerDiagnosticChannels([
  ['http.client.request.created', onClientRequestCreated],
  ['http.client.request.upgrade', onClientRequestUpgrade],
  ['http.client.request.bodyChunkSent', onClientRequestBodyChunkSent],
  ['http.client.request.bodySent', onClientRequestBodySent],
  ['http.client.request.error', onClientRequestError],
  ['http.client.response.bodyChunkReceived', onClientResponseBodyChunkReceived],
  ['http.client.response.finish', onClientResponseFinish],
  ['websocket.client.frameSent', onWebSocketFrameSent],
  ['websocket.client.frameReceived', onWebSocketFrameReceived],
]);
