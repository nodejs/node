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
      zlib.gzip(plainTextBody, (err, compressed) => {
        if (err) throw err;
        res.end(compressed);
      });
      break;
    case '/deflate':
      setResponseHeaders(res, 'deflate');
      res.writeHead(200);
      zlib.deflate(plainTextBody, (err, compressed) => {
        if (err) throw err;
        res.end(compressed);
      });
      break;
    case '/br':
      setResponseHeaders(res, 'br');
      res.writeHead(200);
      zlib.brotliCompress(plainTextBody, (err, compressed) => {
        if (err) throw err;
        res.end(compressed);
      });
      break;
    case '/zstd':
      setResponseHeaders(res, 'zstd');
      res.writeHead(200);
      zlib.zstdCompress(plainTextBody, (err, compressed) => {
        if (err) throw err;
        res.end(compressed);
      });
      break;
    case '/plain':
      setResponseHeaders(res);
      res.writeHead(200);
      res.end(plainTextBody);
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
      if (encoding === 'gzip') {
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

async function testGzipHttp() {
  await testCompressedResponse(httpServer, 'gzip', '/gzip');
}

async function testGzipHttps() {
  await testCompressedResponse(httpsServer, 'gzip', '/gzip');
}

async function testDeflateHttp() {
  await testCompressedResponse(httpServer, 'deflate', '/deflate');
}

async function testDeflateHttps() {
  await testCompressedResponse(httpsServer, 'deflate', '/deflate');
}

async function testBrotliHttp() {
  await testCompressedResponse(httpServer, 'br', '/br');
}

async function testBrotliHttps() {
  await testCompressedResponse(httpsServer, 'br', '/br');
}

async function testZstdHttp() {
  await testCompressedResponse(httpServer, 'zstd', '/zstd');
}

async function testZstdHttps() {
  await testCompressedResponse(httpsServer, 'zstd', '/zstd');
}

async function testPlainHttp() {
  await testCompressedResponse(httpServer, null, '/plain');
}

async function testPlainHttps() {
  await testCompressedResponse(httpsServer, null, '/plain');
}

const testNetworkInspection = async () => {
  // Test gzip
  await testGzipHttp();
  session.removeAllListeners();
  await testGzipHttps();
  session.removeAllListeners();

  // Test deflate
  await testDeflateHttp();
  session.removeAllListeners();
  await testDeflateHttps();
  session.removeAllListeners();

  // Test brotli
  await testBrotliHttp();
  session.removeAllListeners();
  await testBrotliHttps();
  session.removeAllListeners();

  // Test zstd
  await testZstdHttp();
  session.removeAllListeners();
  await testZstdHttps();
  session.removeAllListeners();

  // Test plain (no compression)
  await testPlainHttp();
  session.removeAllListeners();
  await testPlainHttps();
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
