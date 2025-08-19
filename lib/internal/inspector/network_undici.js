'use strict';

const {
  DateNow,
  StringPrototypeToLowerCase,
} = primordials;

const {
  kInspectorRequestId,
  kResourceType,
  getMonotonicTime,
  getNextRequestId,
  sniffMimeType,
} = require('internal/inspector/network');
const dc = require('diagnostics_channel');
const { Network } = require('inspector');
const { Buffer } = require('buffer');

// Convert an undici request headers array to a plain object (Map<string, string>)
function requestHeadersArrayToDictionary(headers) {
  const dict = {};
  let charset;
  let mimeType;
  for (let idx = 0; idx < headers.length; idx += 2) {
    const key = `${headers[idx]}`;
    const value = `${headers[idx + 1]}`;
    dict[key] = value;

    if (StringPrototypeToLowerCase(key) === 'content-type') {
      const result = sniffMimeType(value);
      charset = result.charset;
      mimeType = result.mimeType;
    }
  }
  return [dict, charset, mimeType];
};

// Convert an undici response headers array to a plain object (Map<string, string>)
function responseHeadersArrayToDictionary(headers) {
  const dict = {};
  let charset;
  let mimeType;
  for (let idx = 0; idx < headers.length; idx += 2) {
    const key = `${headers[idx]}`;
    const lowerCasedKey = StringPrototypeToLowerCase(key);
    const value = `${headers[idx + 1]}`;
    const prevValue = dict[key];

    if (lowerCasedKey === 'content-type') {
      const result = sniffMimeType(value);
      charset = result.charset;
      mimeType = result.mimeType;
    }

    if (typeof prevValue === 'string') {
      // ChromeDevTools frontend treats 'set-cookie' as a special case
      // https://github.com/ChromeDevTools/devtools-frontend/blob/4275917f84266ef40613db3c1784a25f902ea74e/front_end/core/sdk/NetworkRequest.ts#L1368
      if (lowerCasedKey === 'set-cookie') dict[key] = `${prevValue}\n${value}`;
      else dict[key] = `${prevValue}, ${value}`;
    } else {
      dict[key] = value;
    }
  }
  return [dict, charset, mimeType];
};

/**
 * When a client request starts, emit Network.requestWillBeSent event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-requestWillBeSent
 * @param {{ request: undici.Request }} event
 */
function onClientRequestStart({ request }) {
  const url = `${request.origin}${request.path}`;
  request[kInspectorRequestId] = getNextRequestId();

  const { 0: headers, 1: charset } = requestHeadersArrayToDictionary(request.headers);

  Network.requestWillBeSent({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    wallTime: DateNow(),
    charset,
    request: {
      url,
      method: request.method,
      headers: headers,
      hasPostData: request.body != null,
    },
  });
}

/**
 * When a client request errors, emit Network.loadingFailed event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-loadingFailed
 * @param {{ request: undici.Request, error: any }} event
 */
function onClientRequestError({ request, error }) {
  if (typeof request[kInspectorRequestId] !== 'string') {
    return;
  }
  Network.loadingFailed({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    // TODO(legendecas): distinguish between `undici.request` and `undici.fetch`.
    type: kResourceType.Fetch,
    errorText: error.message,
  });
}

/**
 * When a chunk of the request body is being sent, cache it until `getRequestPostData` request.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#method-getRequestPostData
 * @param {{ request: undici.Request, chunk: Uint8Array | string }} event
 */
function onClientRequestBodyChunkSent({ request, chunk }) {
  if (typeof request[kInspectorRequestId] !== 'string') {
    return;
  }

  const buffer = Buffer.from(chunk);
  Network.dataSent({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    dataLength: buffer.byteLength,
    data: buffer,
  });
}

/**
 * Mark a request body as fully sent.
 * @param {{request: undici.Request}} event
 */
function onClientRequestBodySent({ request }) {
  if (typeof request[kInspectorRequestId] !== 'string') {
    return;
  }

  Network.dataSent({
    requestId: request[kInspectorRequestId],
    finished: true,
  });
}

/**
 * When response headers are received, emit Network.responseReceived event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-responseReceived
 * @param {{ request: undici.Request, response: undici.Response }} event
 */
function onClientResponseHeaders({ request, response }) {
  if (typeof request[kInspectorRequestId] !== 'string') {
    return;
  }

  const { 0: headers, 1: charset, 2: mimeType } = responseHeadersArrayToDictionary(response.headers);

  const url = `${request.origin}${request.path}`;
  Network.responseReceived({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    // TODO(legendecas): distinguish between `undici.request` and `undici.fetch`.
    type: kResourceType.Fetch,
    response: {
      url,
      status: response.statusCode,
      statusText: response.statusText,
      headers,
      mimeType,
      charset,
    },
  });
}

/**
 * When a chunk of the response body has been received, cache it until `getResponseBody` request
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#method-getResponseBody or
 * stream it with `streamResourceContent` request.
 * https://chromedevtools.github.io/devtools-protocol/tot/Network/#method-streamResourceContent
 * @param {{ request: undici.Request, chunk: Uint8Array | string }} event
 */
function onClientRequestBodyChunkReceived({ request, chunk }) {
  if (typeof request[kInspectorRequestId] !== 'string') {
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
 * When a response is completed, emit Network.loadingFinished event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-loadingFinished
 * @param {{ request: undici.Request, response: undici.Response }} event
 */
function onClientResponseFinish({ request }) {
  if (typeof request[kInspectorRequestId] !== 'string') {
    return;
  }
  Network.loadingFinished({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
  });
}

// TODO: Move Network.webSocketCreated to the actual creation time of the WebSocket.
// undici:websocket:open fires when the connection is established, but this results
// in an inaccurate stack trace.
function onWebSocketOpen({ websocket }) {
  websocket[kInspectorRequestId] = getNextRequestId();
  const url = websocket.url.toString();
  Network.webSocketCreated({
    requestId: websocket[kInspectorRequestId],
    url,
  });
  // TODO: Use handshake response data from undici diagnostics when available.
  // https://github.com/nodejs/undici/pull/4396
  Network.webSocketHandshakeResponseReceived({
    requestId: websocket[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    response: {
      status: 101,
      statusText: 'Switching Protocols',
      headers: {},
    },
  });
}

function onWebSocketClose({ websocket }) {
  if (typeof websocket[kInspectorRequestId] !== 'string') {
    return;
  }
  Network.webSocketClosed({
    requestId: websocket[kInspectorRequestId],
    timestamp: getMonotonicTime(),
  });
}

function enable() {
  dc.subscribe('undici:request:create', onClientRequestStart);
  dc.subscribe('undici:request:error', onClientRequestError);
  dc.subscribe('undici:request:headers', onClientResponseHeaders);
  dc.subscribe('undici:request:trailers', onClientResponseFinish);
  dc.subscribe('undici:request:bodyChunkSent', onClientRequestBodyChunkSent);
  dc.subscribe('undici:request:bodySent', onClientRequestBodySent);
  dc.subscribe('undici:request:bodyChunkReceived', onClientRequestBodyChunkReceived);
  dc.subscribe('undici:websocket:open', onWebSocketOpen);
  dc.subscribe('undici:websocket:close', onWebSocketClose);
}

function disable() {
  dc.unsubscribe('undici:request:create', onClientRequestStart);
  dc.unsubscribe('undici:request:error', onClientRequestError);
  dc.unsubscribe('undici:request:headers', onClientResponseHeaders);
  dc.unsubscribe('undici:request:trailers', onClientResponseFinish);
  dc.unsubscribe('undici:request:bodyChunkSent', onClientRequestBodyChunkSent);
  dc.unsubscribe('undici:request:bodySent', onClientRequestBodySent);
  dc.unsubscribe('undici:request:bodyChunkReceived', onClientRequestBodyChunkReceived);
  dc.unsubscribe('undici:websocket:open', onWebSocketOpen);
  dc.unsubscribe('undici:websocket:close', onWebSocketClose);
}

module.exports = {
  enable,
  disable,
};
