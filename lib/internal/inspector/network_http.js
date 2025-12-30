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
  kContentEncoding,
  kResourceType,
  getMonotonicTime,
  getNextRequestId,
  registerDiagnosticChannels,
  sniffMimeType,
  createDecompressor,
} = require('internal/inspector/network');
const { Network } = require('inspector');
const EventEmitter = require('events');

const kRequestUrl = Symbol('kRequestUrl');

// Convert a Headers object (Map<string, number | string | string[]>) to a plain object (Map<string, string>)
const convertHeaderObject = (headers = {}) => {
  // The 'host' header that contains the host and port of the URL.
  let host;
  let charset;
  let mimeType;
  let contentEncoding;
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
    if (lowerCasedKey === 'content-encoding') {
      contentEncoding = typeof value === 'string' ? value.toLowerCase() : undefined;
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
  return [dict, host, charset, mimeType, contentEncoding];
};

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

/**
 * When a client request errors, emit Network.loadingFailed event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-loadingFailed
 * @param {{ request: import('http').ClientRequest, error: any }} event
 */
function onClientRequestError({ request, error }) {
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
 * When response headers are received, emit Network.responseReceived event.
 * https://chromedevtools.github.io/devtools-protocol/1-3/Network/#event-responseReceived
 * @param {{ request: import('http').ClientRequest, error: any }} event
 */
function onClientResponseFinish({ request, response }) {
  if (typeof request[kInspectorRequestId] !== 'string') {
    return;
  }

  const { 0: headers, 2: charset, 3: mimeType, 4: contentEncoding } = convertHeaderObject(response.headers);

  // Store content encoding on the request for later use
  request[kContentEncoding] = contentEncoding;

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

  // Create a decompressor if the response is compressed
  const decompressor = createDecompressor(contentEncoding);

  if (decompressor) {
    // Pipe decompressed data to DevTools
    decompressor.on('data', (decompressedChunk) => {
      Network.dataReceived({
        requestId: request[kInspectorRequestId],
        timestamp: getMonotonicTime(),
        dataLength: decompressedChunk.byteLength,
        encodedDataLength: decompressedChunk.byteLength,
        data: decompressedChunk,
      });
    });

    // Handle decompression errors gracefully - fall back to raw data
    decompressor.on('error', () => {
      // If decompression fails, the raw data has already been sent via the fallback
    });

    // Unlike response.on('data', ...), this does not put the stream into flowing mode.
    EventEmitter.prototype.on.call(response, 'data', (chunk) => {
      // Feed the chunk into the decompressor
      decompressor.write(chunk);
    });

    // Wait until the response body is consumed by user code.
    response.once('end', () => {
      // End the decompressor stream
      decompressor.end();
      decompressor.once('end', () => {
        Network.loadingFinished({
          requestId: request[kInspectorRequestId],
          timestamp: getMonotonicTime(),
        });
      });
    });
  } else {
    // No decompression needed, send data directly
    // Unlike response.on('data', ...), this does not put the stream into flowing mode.
    EventEmitter.prototype.on.call(response, 'data', (chunk) => {
      Network.dataReceived({
        requestId: request[kInspectorRequestId],
        timestamp: getMonotonicTime(),
        dataLength: chunk.byteLength,
        encodedDataLength: chunk.byteLength,
        data: chunk,
      });
    });

    // Wait until the response body is consumed by user code.
    response.once('end', () => {
      Network.loadingFinished({
        requestId: request[kInspectorRequestId],
        timestamp: getMonotonicTime(),
      });
    });
  }
}

module.exports = registerDiagnosticChannels([
  ['http.client.request.created', onClientRequestCreated],
  ['http.client.request.error', onClientRequestError],
  ['http.client.response.finish', onClientResponseFinish],
]);
