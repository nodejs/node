'use strict';

const {
  DateNow,
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

// Convert an undici request headers array to a plain object (Map<string, string>)
function requestHeadersArrayToDictionary(headers) {
  const dict = {};
  for (let idx = 0; idx < headers.length; idx += 2) {
    const key = `${headers[idx]}`;
    const value = `${headers[idx + 1]}`;
    dict[key] = value;
  }
  return dict;
};

// Convert an undici response headers array to a plain object (Map<string, string>)
function responseHeadersArrayToDictionary(headers) {
  const dict = {};
  let charset;
  let mimeType;
  for (let idx = 0; idx < headers.length; idx += 2) {
    const key = `${headers[idx]}`;
    const lowerCasedKey = key.toLowerCase();
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

  const headers = requestHeadersArrayToDictionary(request.headers);
  const { charset } = sniffMimeType(headers);

  Network.requestWillBeSent({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    wallTime: DateNow(),
    charset,
    request: {
      url,
      method: request.method,
      headers: requestHeadersArrayToDictionary(request.headers),
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

function enable() {
  dc.subscribe('undici:request:create', onClientRequestStart);
  dc.subscribe('undici:request:error', onClientRequestError);
  dc.subscribe('undici:request:headers', onClientResponseHeaders);
  dc.subscribe('undici:request:trailers', onClientResponseFinish);
}

function disable() {
  dc.unsubscribe('undici:request:create', onClientRequestStart);
  dc.unsubscribe('undici:request:error', onClientRequestError);
  dc.unsubscribe('undici:request:headers', onClientResponseHeaders);
  dc.unsubscribe('undici:request:trailers', onClientResponseFinish);
}

module.exports = {
  enable,
  disable,
};
