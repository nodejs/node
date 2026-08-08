// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
common.skipIfInspectorDisabled();

const assert = require('node:assert');
const { once } = require('node:events');
const fixtures = require('../common/fixtures');
const http2 = require('node:http2');
const zlib = require('node:zlib');
const inspector = require('node:inspector/promises');

const session = new inspector.Session();
session.connect();

const plainTextBody = 'hello world compressed http2\n';

const handleStream = common.mustCallAtLeast((stream, headers) => {
  const path = headers[http2.constants.HTTP2_HEADER_PATH];

  const responseHeaders = {
    [http2.constants.HTTP2_HEADER_STATUS]: 200,
    [http2.constants.HTTP2_HEADER_CONTENT_TYPE]: 'text/plain; charset=utf-8',
  };

  switch (path) {
    case '/gzip':
      responseHeaders[http2.constants.HTTP2_HEADER_CONTENT_ENCODING] = 'gzip';
      stream.respond(responseHeaders);
      zlib.gzip(plainTextBody, common.mustSucceed((compressed) => {
        stream.end(compressed);
      }));
      break;
    case '/x-gzip':
      responseHeaders[http2.constants.HTTP2_HEADER_CONTENT_ENCODING] = 'x-gzip';
      stream.respond(responseHeaders);
      zlib.gzip(plainTextBody, common.mustSucceed((compressed) => {
        stream.end(compressed);
      }));
      break;
    case '/deflate':
      responseHeaders[http2.constants.HTTP2_HEADER_CONTENT_ENCODING] = 'deflate';
      stream.respond(responseHeaders);
      zlib.deflate(plainTextBody, common.mustSucceed((compressed) => {
        stream.end(compressed);
      }));
      break;
    case '/br':
      responseHeaders[http2.constants.HTTP2_HEADER_CONTENT_ENCODING] = 'br';
      stream.respond(responseHeaders);
      zlib.brotliCompress(plainTextBody, common.mustSucceed((compressed) => {
        stream.end(compressed);
      }));
      break;
    case '/zstd':
      responseHeaders[http2.constants.HTTP2_HEADER_CONTENT_ENCODING] = 'zstd';
      stream.respond(responseHeaders);
      zlib.zstdCompress(plainTextBody, common.mustSucceed((compressed) => {
        stream.end(compressed);
      }));
      break;
    case '/plain':
      stream.respond(responseHeaders);
      stream.end(plainTextBody);
      break;
    case '/invalid-gzip':
      // Send invalid data with gzip content-encoding to trigger decompression error
      responseHeaders[http2.constants.HTTP2_HEADER_CONTENT_ENCODING] = 'gzip';
      stream.respond(responseHeaders);
      stream.end('this is not valid gzip data');
      break;
    default:
      assert.fail(`Unexpected path: ${path}`);
  }
});

const http2Server = http2.createServer();
const http2SecureServer = http2.createSecureServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
});

http2Server.on('stream', handleStream);
http2SecureServer.on('stream', handleStream);

const terminate = () => {
  session.disconnect();
  http2Server.close();
  http2SecureServer.close();
  inspector.close();
};

function verifyResponseReceived({ method, params }, expect) {
  assert.strictEqual(method, 'Network.responseReceived');
  assert.ok(params.requestId.startsWith('node-network-event-'));
  assert.strictEqual(params.response.status, 200);
  assert.strictEqual(params.response.url, expect.url);
  assert.strictEqual(params.response.mimeType, 'text/plain');
  assert.strictEqual(params.response.charset, 'utf-8');
  return params;
}

function verifyLoadingFinished({ method, params }) {
  assert.strictEqual(method, 'Network.loadingFinished');
  assert.ok(params.requestId.startsWith('node-network-event-'));
  return params;
}

async function testInvalidCompressedResponse(server) {
  const port = server.address().port;
  const secure = server === http2SecureServer;
  const origin = (secure ? 'https' : 'http') + `://localhost:${port}`;
  const path = '/invalid-gzip';
  const url = `${origin}${path}`;

  const responseReceivedFuture = once(session, 'Network.responseReceived')
    .then(([event]) => verifyResponseReceived(event, { url }));

  await new Promise((resolve) => {
    const client = http2.connect(origin, {
      rejectUnauthorized: false,
    });

    const req = client.request({
      [http2.constants.HTTP2_HEADER_PATH]: path,
      [http2.constants.HTTP2_HEADER_METHOD]: 'GET',
    });

    // Consume the response to trigger the decompression error in inspector
    req.on('data', () => {});
    req.on('end', () => {
      client.close();
      resolve();
    });
    req.on('error', () => {
      client.close();
      resolve();
    });
    req.end();
  });

  await responseReceivedFuture;
  // Note: loadingFinished is not emitted when decompression fails,
  // but this test ensures the error handler is triggered for coverage.
}

async function testCompressedResponse(server, encoding, path) {
  const port = server.address().port;
  const secure = server === http2SecureServer;
  const origin = (secure ? 'https' : 'http') + `://localhost:${port}`;
  const url = `${origin}${path}`;

  const responseReceivedFuture = once(session, 'Network.responseReceived')
    .then(([event]) => verifyResponseReceived(event, { url }));

  const loadingFinishedFuture = once(session, 'Network.loadingFinished')
    .then(([event]) => verifyLoadingFinished(event));

  const chunks = [];

  await new Promise((resolve, reject) => {
    const client = http2.connect(origin, {
      rejectUnauthorized: false,
    });

    const req = client.request({
      [http2.constants.HTTP2_HEADER_PATH]: path,
      [http2.constants.HTTP2_HEADER_METHOD]: 'GET',
    });

    // Manually decompress the response to verify it works for user code
    let decompressor;
    if (encoding === 'gzip' || encoding === 'x-gzip') {
      decompressor = zlib.createGunzip();
    } else if (encoding === 'deflate') {
      decompressor = zlib.createInflate();
    } else if (encoding === 'br') {
      decompressor = zlib.createBrotliDecompress();
    } else if (encoding === 'zstd') {
      decompressor = zlib.createZstdDecompress();
    }

    if (decompressor) {
      req.pipe(decompressor);
      decompressor.on('data', (chunk) => chunks.push(chunk));
      decompressor.on('end', () => {
        client.close();
        resolve();
      });
      decompressor.on('error', (err) => {
        client.close();
        reject(err);
      });
    } else {
      req.on('data', (chunk) => chunks.push(chunk));
      req.on('end', () => {
        client.close();
        resolve();
      });
    }

    req.on('error', (err) => {
      client.close();
      reject(err);
    });
    req.end();
  });

  // Verify user code can read the decompressed response
  const body = Buffer.concat(chunks).toString();
  assert.strictEqual(body, plainTextBody);

  const responseReceived = await responseReceivedFuture;
  await loadingFinishedFuture;

  // Verify the inspector receives the decompressed response body
  const responseBody = await session.post('Network.getResponseBody', {
    requestId: responseReceived.requestId,
  });
  assert.strictEqual(responseBody.base64Encoded, false);
  assert.strictEqual(responseBody.body, plainTextBody);
}

const testNetworkInspection = async () => {
  // Test gzip
  await testCompressedResponse(http2Server, 'gzip', '/gzip');
  session.removeAllListeners();
  await testCompressedResponse(http2SecureServer, 'gzip', '/gzip');
  session.removeAllListeners();

  // Test x-gzip (alternate gzip encoding)
  await testCompressedResponse(http2Server, 'x-gzip', '/x-gzip');
  session.removeAllListeners();
  await testCompressedResponse(http2SecureServer, 'x-gzip', '/x-gzip');
  session.removeAllListeners();

  // Test deflate
  await testCompressedResponse(http2Server, 'deflate', '/deflate');
  session.removeAllListeners();
  await testCompressedResponse(http2SecureServer, 'deflate', '/deflate');
  session.removeAllListeners();

  // Test brotli
  await testCompressedResponse(http2Server, 'br', '/br');
  session.removeAllListeners();
  await testCompressedResponse(http2SecureServer, 'br', '/br');
  session.removeAllListeners();

  // Test zstd
  await testCompressedResponse(http2Server, 'zstd', '/zstd');
  session.removeAllListeners();
  await testCompressedResponse(http2SecureServer, 'zstd', '/zstd');
  session.removeAllListeners();

  // Test plain (no compression)
  await testCompressedResponse(http2Server, null, '/plain');
  session.removeAllListeners();
  await testCompressedResponse(http2SecureServer, null, '/plain');
  session.removeAllListeners();

  // Test invalid compressed data (triggers decompression error handler)
  await testInvalidCompressedResponse(http2Server);
  session.removeAllListeners();
  await testInvalidCompressedResponse(http2SecureServer);
  session.removeAllListeners();
};

http2Server.listen(0, () => {
  http2SecureServer.listen(0, async () => {
    try {
      await session.post('Network.enable');
      await testNetworkInspection();
      await session.post('Network.disable');
    } catch (e) {
      assert.fail(e);
    } finally {
      terminate();
    }
  });
});
