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
  sniffMimeType,
} = require('internal/inspector/network');
const dc = require('diagnostics_channel');
const { Network } = require('inspector');
const {
  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_COOKIE,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_SET_COOKIE,
  HTTP2_HEADER_STATUS,
  NGHTTP2_NO_ERROR,
} = internalBinding('http2').constants;

const kRequestUrl = Symbol('kRequestUrl');

// Convert a Headers object (Map<string, number | string | string[]>) to a plain object (Map<string, string>)
function convertHeaderObject(headers = {}) {
  let scheme;
  let authority;
  let path;
  let method;
  let statusCode;
  let charset;
  let mimeType;
  const dict = {};

  for (const { 0: key, 1: value } of ObjectEntries(headers)) {
    const lowerCasedKey = key.toLowerCase();

    if (lowerCasedKey === HTTP2_HEADER_SCHEME) {
      scheme = value;
    } else if (lowerCasedKey === HTTP2_HEADER_AUTHORITY) {
      authority = value;
    } else if (lowerCasedKey === HTTP2_HEADER_PATH) {
      path = value;
    } else if (lowerCasedKey === HTTP2_HEADER_METHOD) {
      method = value;
    } else if (lowerCasedKey === HTTP2_HEADER_STATUS) {
      statusCode = value;
    } else if (lowerCasedKey === HTTP2_HEADER_CONTENT_TYPE) {
      const result = sniffMimeType(value);
      charset = result.charset;
      mimeType = result.mimeType;
    }

    if (typeof value === 'string') {
      dict[key] = value;
    } else if (ArrayIsArray(value)) {
      if (lowerCasedKey === HTTP2_HEADER_COOKIE) dict[key] = value.join('; ');
      // ChromeDevTools frontend treats 'set-cookie' as a special case
      // https://github.com/ChromeDevTools/devtools-frontend/blob/4275917f84266ef40613db3c1784a25f902ea74e/front_end/core/sdk/NetworkRequest.ts#L1368
      else if (lowerCasedKey === HTTP2_HEADER_SET_COOKIE) dict[key] = value.join('\n');
      else dict[key] = value.join(', ');
    } else {
      dict[key] = String(value);
    }
  }

  const url = `${scheme}://${authority}${path}`;

  return [dict, url, method, statusCode, charset, mimeType];
}

/**
 * When a client stream is created, emit Network.requestWillBeSent event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-requestWillBeSent
 * @param {{ stream: import('http2').ClientHttp2Stream, headers: object }} event
 */
function onClientStreamCreated({ stream, headers }) {
  stream[kInspectorRequestId] = getNextRequestId();

  const { 0: convertedHeaderObject, 1: url, 2: method, 4: charset } = convertHeaderObject(headers);
  stream[kRequestUrl] = url;

  Network.requestWillBeSent({
    requestId: stream[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    wallTime: DateNow(),
    charset,
    request: {
      url,
      method,
      headers: convertedHeaderObject,
    },
  });
}

/**
 * When a client stream errors, emit Network.loadingFailed event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-loadingFailed
 * @param {{ stream: import('http2').ClientHttp2Stream, error: any }} event
 */
function onClientStreamError({ stream, error }) {
  if (typeof stream[kInspectorRequestId] !== 'string') {
    return;
  }

  Network.loadingFailed({
    requestId: stream[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    type: kResourceType.Other,
    errorText: error.message,
  });
}

/**
 * When response headers are received, emit Network.responseReceived event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-responseReceived
 * @param {{ stream: import('http2').ClientHttp2Stream, headers: object }} event
 */
function onClientStreamFinish({ stream, headers }) {
  if (typeof stream[kInspectorRequestId] !== 'string') {
    return;
  }

  const { 0: convertedHeaderObject, 3: statusCode, 4: charset, 5: mimeType } = convertHeaderObject(headers);

  Network.responseReceived({
    requestId: stream[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    type: kResourceType.Other,
    response: {
      url: stream[kRequestUrl],
      status: statusCode,
      statusText: '',
      headers: convertedHeaderObject,
      mimeType,
      charset,
    },
  });
}

/**
 * When user code completes consuming the response body, emit Network.loadingFinished event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-loadingFinished
 * @param {{ stream: import('http2').ClientHttp2Stream }} event
 */
function onClientStreamClose({ stream }) {
  if (typeof stream[kInspectorRequestId] !== 'string') {
    return;
  }

  if (stream.rstCode !== NGHTTP2_NO_ERROR) {
    // This is an error case, so only Network.loadingFailed should be emitted
    // which is already done by onClientStreamError().
    return;
  }

  Network.loadingFinished({
    requestId: stream[kInspectorRequestId],
    timestamp: getMonotonicTime(),
  });
}

function enable() {
  dc.subscribe('http2.client.stream.created', onClientStreamCreated);
  dc.subscribe('http2.client.stream.error', onClientStreamError);
  dc.subscribe('http2.client.stream.finish', onClientStreamFinish);
  dc.subscribe('http2.client.stream.close', onClientStreamClose);
}

function disable() {
  dc.unsubscribe('http2.client.stream.created', onClientStreamCreated);
  dc.unsubscribe('http2.client.stream.error', onClientStreamError);
  dc.unsubscribe('http2.client.stream.finish', onClientStreamFinish);
  dc.unsubscribe('http2.client.stream.close', onClientStreamClose);
}

module.exports = {
  enable,
  disable,
};
