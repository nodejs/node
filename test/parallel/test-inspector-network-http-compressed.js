// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('node:assert');
const { once } = require('node:events');
const fixtures = require('../common/fixtures');
const http = require('node:http');
const https = require('node:https');
const zlib = require('node:zlib');
const inspector = require('node:inspector/promises');

const session = new inspector.Session();
session.connect();

const plainTextBody = 'hello world compressed\n';

const setResponseHeaders = (res, encoding) => {
  res.setHeader('server', 'node');
  res.setHeader('Content-Type', 'text/plain; charset=utf-8');
  if (encoding) {
    res.setHeader('Content-Encoding', encoding);
  }
};

const handleRequest = (req, res) => {
  const path = req.url;
  switch (path) {
    case '/gzip':
      setResponseHeaders(res, 'gzip');
      res.writeHead(200);
      zlib.gzip(plainTextBody, common.mustSucceed((compressed) => {
        res.end(compressed);
      }));
      break;
    case '/x-gzip':
      setResponseHeaders(res, 'x-gzip');
      res.writeHead(200);
      zlib.gzip(plainTextBody, common.mustSucceed((compressed) => {
        res.end(compressed);
      }));
      break;
    case '/deflate':
      setResponseHeaders(res, 'deflate');
      res.writeHead(200);
      zlib.deflate(plainTextBody, common.mustSucceed((compressed) => {
        res.end(compressed);
      }));
      break;
    case '/br':
      setResponseHeaders(res, 'br');
      res.writeHead(200);
      zlib.brotliCompress(plainTextBody, common.mustSucceed((compressed) => {
        res.end(compressed);
      }));
      break;
    case '/zstd':
      setResponseHeaders(res, 'zstd');
      res.writeHead(200);
      zlib.zstdCompress(plainTextBody, common.mustSucceed((compressed) => {
        res.end(compressed);
      }));
      break;
    case '/plain':
      setResponseHeaders(res);
      res.writeHead(200);
      res.end(plainTextBody);
      break;
    case '/invalid-gzip':
      // Send invalid data with gzip content-encoding to trigger decompression error
      setResponseHeaders(res, 'gzip');
      res.writeHead(200);
      res.end('this is not valid gzip data');
      break;
    default:
      assert.fail(`Unexpected path: ${path}`);
  }
};

const httpServer = http.createServer(handleRequest);

const httpsServer = https.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
}, handleRequest);

const terminate = () => {
  session.disconnect();
  httpServer.close();
  httpsServer.close();
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
  const protocol = server === httpsServer ? 'https' : 'http';
  const path = '/invalid-gzip';
  const url = `${protocol}://127.0.0.1:${port}${path}`;

  const responseReceivedFuture = once(session, 'Network.responseReceived')
    .then(([event]) => verifyResponseReceived(event, { url }));

  const client = protocol === 'https' ? https : http;

  await new Promise((resolve) => {
    const req = client.get({
      host: '127.0.0.1',
      port,
      path,
      rejectUnauthorized: false,
    }, (res) => {
      // Consume the response to trigger the decompression error in inspector
      res.on('data', () => {});
      res.on('end', resolve);
    });
    req.on('error', resolve);
  });

  await responseReceivedFuture;
  // Note: loadingFinished is not emitted when decompression fails,
  // but this test ensures the error handler is triggered for coverage.
}

async function testCompressedResponse(server, encoding, path) {
  const port = server.address().port;
  const protocol = server === httpsServer ? 'https' : 'http';
  const url = `${protocol}://127.0.0.1:${port}${path}`;

  const responseReceivedFuture = once(session, 'Network.responseReceived')
    .then(([event]) => verifyResponseReceived(event, { url }));

  const loadingFinishedFuture = once(session, 'Network.loadingFinished')
    .then(([event]) => verifyLoadingFinished(event));

  const client = protocol === 'https' ? https : http;
  const chunks = [];

  await new Promise((resolve, reject) => {
    const req = client.get({
      host: '127.0.0.1',
      port,
      path,
      rejectUnauthorized: false,
    }, (res) => {
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
        res.pipe(decompressor);
        decompressor.on('data', (chunk) => chunks.push(chunk));
        decompressor.on('end', resolve);
        decompressor.on('error', reject);
      } else {
        res.on('data', (chunk) => chunks.push(chunk));
        res.on('end', resolve);
      }
    });
    req.on('error', reject);
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
  await testCompressedResponse(httpServer, 'gzip', '/gzip');
  session.removeAllListeners();
  await testCompressedResponse(httpsServer, 'gzip', '/gzip');
  session.removeAllListeners();

  // Test x-gzip (alternate gzip encoding)
  await testCompressedResponse(httpServer, 'x-gzip', '/x-gzip');
  session.removeAllListeners();
  await testCompressedResponse(httpsServer, 'x-gzip', '/x-gzip');
  session.removeAllListeners();

  // Test deflate
  await testCompressedResponse(httpServer, 'deflate', '/deflate');
  session.removeAllListeners();
  await testCompressedResponse(httpsServer, 'deflate', '/deflate');
  session.removeAllListeners();

  // Test brotli
  await testCompressedResponse(httpServer, 'br', '/br');
  session.removeAllListeners();
  await testCompressedResponse(httpsServer, 'br', '/br');
  session.removeAllListeners();

  // Test zstd
  await testCompressedResponse(httpServer, 'zstd', '/zstd');
  session.removeAllListeners();
  await testCompressedResponse(httpsServer, 'zstd', '/zstd');
  session.removeAllListeners();

  // Test plain (no compression)
  await testCompressedResponse(httpServer, null, '/plain');
  session.removeAllListeners();
  await testCompressedResponse(httpsServer, null, '/plain');
  session.removeAllListeners();

  // Test invalid compressed data (triggers decompression error handler)
  await testInvalidCompressedResponse(httpServer);
  session.removeAllListeners();
  await testInvalidCompressedResponse(httpsServer);
  session.removeAllListeners();
};

httpServer.listen(0, () => {
  httpsServer.listen(0, async () => {
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
