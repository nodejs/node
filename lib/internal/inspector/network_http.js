'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  DateNow,
  MathMax,
  ObjectEntries,
  String,
  StringPrototypeStartsWith,
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
const {
  getStructuredStack,
  kEmptyObject,
} = require('internal/util');

const kRequestUrl = Symbol('kRequestUrl');
const kRequestWillBeSent = Symbol('kRequestWillBeSent');
const kInitiator = Symbol('kInitiator');

function isAbsoluteURLPath(path) {
  return typeof path === 'string' &&
    (StringPrototypeStartsWith(path, 'http://') ||
     StringPrototypeStartsWith(path, 'https://'));
}

function getRequestURL(request, host) {
  if (isAbsoluteURLPath(request.path)) {
    return request.path;
  }
  return `${request.protocol}//${host}${request.path}`;
}

// Convert a Headers object (Map<string, number | string | string[]>) to a plain object (Map<string, string>)
const convertHeaderObject = (headers = kEmptyObject) => {
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

function getCallSiteUrl(callSite) {
  return callSite.getScriptNameOrSourceURL() ?? callSite.getFileName() ?? '';
}

function isInternalCallSite(callSite) {
  const url = getCallSiteUrl(callSite);
  return url === 'structured-stack' || StringPrototypeStartsWith(url, 'node:');
}

function createInitiator() {
  const callSites = getStructuredStack();
  const callFrames = [];
  let start = 0;

  for (let i = 0; i < callSites.length; i++) {
    if (!isInternalCallSite(callSites[i])) {
      start = i;
      break;
    }
  }

  for (let i = start; i < callSites.length; i++) {
    const callSite = callSites[i];
    ArrayPrototypePush(callFrames, {
      functionName: callSite.getFunctionName() ?? callSite.getMethodName() ?? '',
      scriptId: '',
      url: getCallSiteUrl(callSite),
      lineNumber: MathMax((callSite.getLineNumber() ?? 1) - 1, 0),
      columnNumber: MathMax((callSite.getColumnNumber() ?? 1) - 1, 0),
    });
  }

  return {
    type: 'script',
    stack: { callFrames },
  };
}

/**
 * When a client request is created, assign its inspector request id.
 * @param {{ request: import('http').ClientRequest }} event
 */
function onClientRequestCreated({ request }) {
  request[kInspectorRequestId] = getNextRequestId();
  request[kInitiator] = createInitiator();
}

/**
 * Emit Network.requestWillBeSent once the request body state is known.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-requestWillBeSent
 * @param {import('http').ClientRequest} request
 * @param {boolean} hasPostData
 */
function emitRequestWillBeSent(request, hasPostData) {
  if (request[kRequestWillBeSent] ||
      typeof request[kInspectorRequestId] !== 'string') {
    return;
  }

  const { 0: headers, 1: host, 2: charset } = convertHeaderObject(request.getHeaders());
  const url = getRequestURL(request, host);
  request[kRequestUrl] = url;
  request[kRequestWillBeSent] = true;

  Network.requestWillBeSent({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    wallTime: DateNow(),
    charset,
    initiator: request[kInitiator],
    request: {
      url,
      method: request.method,
      headers,
      hasPostData,
    },
  });
}

/**
 * When a client request starts without a body, emit Network.requestWillBeSent.
 * @param {{ request: import('http').ClientRequest }} event
 */
function onClientRequestStart({ request }) {
  emitRequestWillBeSent(request, false);
}

/**
 * When a client request errors, emit Network.loadingFailed event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-loadingFailed
 * @param {{ request: import('http').ClientRequest, error: any }} event
 */
function onClientRequestError({ request, error }) {
  emitRequestWillBeSent(request, false);
  if (typeof request[kInspectorRequestId] !== 'string') {
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
  if (typeof request[kInspectorRequestId] !== 'string') {
    return;
  }

  emitRequestWillBeSent(request, true);

  const buffer = typeof chunk === 'string' ?
    Buffer.from(chunk, encoding ?? undefined) :
    Buffer.from(chunk);
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
  if (typeof request[kInspectorRequestId] !== 'string') {
    return;
  }

  Network.dataSent({
    requestId: request[kInspectorRequestId],
    timestamp: getMonotonicTime(),
    dataLength: 0,
    data: Buffer.alloc(0),
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
 * When response headers are received, emit Network.responseReceived event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-responseReceived
 * @param {{ request: import('http').ClientRequest, error: any }} event
 */
function onClientResponseFinish({ request, response }) {
  emitRequestWillBeSent(request, false);
  if (typeof request[kInspectorRequestId] !== 'string') {
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
  ['http.client.request.start', onClientRequestStart],
  ['http.client.request.bodyChunkSent', onClientRequestBodyChunkSent],
  ['http.client.request.bodySent', onClientRequestBodySent],
  ['http.client.request.error', onClientRequestError],
  ['http.client.response.bodyChunkReceived', onClientResponseBodyChunkReceived],
  ['http.client.response.finish', onClientResponseFinish],
]);
