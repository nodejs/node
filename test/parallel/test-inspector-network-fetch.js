// Flags: --inspect=0 --experimental-network-inspection --expose-internals
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('node:assert');
const { once } = require('node:events');
const { addresses } = require('../common/internet');
const fixtures = require('../common/fixtures');
const http = require('node:http');
const https = require('node:https');
const inspector = require('node:inspector/promises');

// Disable certificate validation for the global fetch.
const undici = require('internal/deps/undici/undici');
undici.setGlobalDispatcher(new undici.EnvHttpProxyAgent({
  connect: {
    rejectUnauthorized: false,
  },
}));

const session = new inspector.Session();
session.connect();

const requestHeaders = [
  ['accept-language', 'en-US'],
  ['cookie', 'k1=v1'],
  ['cookie', 'k2=v2'],
  ['age', 1000],
  ['x-header1', 'value1'],
  ['x-header1', 'value2'],
];

const setResponseHeaders = (res) => {
  res.setHeader('server', 'node');
  res.setHeader('etag', 12345);
  res.setHeader('Set-Cookie', ['key1=value1', 'key2=value2']);
  res.setHeader('x-header2', ['value1', 'value2']);
  res.setHeader('Content-Type', 'text/plain; charset=utf-8');
};

const handleRequest = (req, res) => {
  const path = req.url;
  switch (path) {
    case '/hello-world': {
      setResponseHeaders(res);
      const chunks = [];
      req.on('data', (chunk) => {
        chunks.push(chunk);
      });
      req.on('end', () => {
        assert.strictEqual(Buffer.concat(chunks).toString(), 'foobar');
        res.writeHead(200);
        res.end('hello world\n');
      });
      break;
    }
    default:
      assert(false, `Unexpected path: ${path}`);
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

function findFrameInInitiator(scriptName, initiator) {
  const frame = initiator.stack.callFrames.find((it) => {
    return it.url === scriptName;
  });
  return frame;
}

function verifyRequestWillBeSent({ method, params }, expect) {
  assert.strictEqual(method, 'Network.requestWillBeSent');

  assert.ok(params.requestId.startsWith('node-network-event-'));
  assert.strictEqual(params.request.url, expect.url);
  assert.strictEqual(params.request.method, expect.method);
  assert.strictEqual(typeof params.request.headers, 'object');
  assert.strictEqual(params.request.headers['accept-language'], 'en-US');
  assert.strictEqual(params.request.headers.cookie, 'k1=v1; k2=v2');
  assert.strictEqual(params.request.headers.age, '1000');
  assert.strictEqual(params.request.headers['x-header1'], 'value1, value2');
  assert.strictEqual(typeof params.timestamp, 'number');
  assert.strictEqual(typeof params.wallTime, 'number');

  assert.strictEqual(typeof params.initiator, 'object');
  assert.strictEqual(params.initiator.type, 'script');
  assert.ok(findFrameInInitiator(__filename, params.initiator));

  return params;
}

function verifyResponseReceived({ method, params }, expect) {
  assert.strictEqual(method, 'Network.responseReceived');

  assert.ok(params.requestId.startsWith('node-network-event-'));
  assert.strictEqual(typeof params.timestamp, 'number');
  assert.strictEqual(params.type, 'Fetch');
  assert.strictEqual(params.response.status, 200);
  assert.strictEqual(params.response.statusText, 'OK');
  assert.strictEqual(params.response.url, expect.url);
  assert.strictEqual(typeof params.response.headers, 'object');
  assert.strictEqual(params.response.headers.server, 'node');
  assert.strictEqual(params.response.headers.etag, '12345');
  assert.strictEqual(params.response.headers['Set-Cookie'], 'key1=value1\nkey2=value2');
  assert.strictEqual(params.response.headers['x-header2'], 'value1, value2');
  assert.strictEqual(params.response.mimeType, 'text/plain');
  assert.strictEqual(params.response.charset, 'utf-8');

  return params;
}

function verifyLoadingFinished({ method, params }) {
  assert.strictEqual(method, 'Network.loadingFinished');

  assert.ok(params.requestId.startsWith('node-network-event-'));
  assert.strictEqual(typeof params.timestamp, 'number');
  return params;
}

function verifyLoadingFailed({ method, params }) {
  assert.strictEqual(method, 'Network.loadingFailed');

  assert.ok(params.requestId.startsWith('node-network-event-'));
  assert.strictEqual(typeof params.timestamp, 'number');
  assert.strictEqual(params.type, 'Fetch');
  assert.strictEqual(typeof params.errorText, 'string');
}

async function testRequest(url) {
  const requestWillBeSentFuture = once(session, 'Network.requestWillBeSent')
    .then(([event]) => verifyRequestWillBeSent(event, { url, method: 'POST' }));

  const responseReceivedFuture = once(session, 'Network.responseReceived')
    .then(([event]) => verifyResponseReceived(event, { url }));

  const loadingFinishedFuture = once(session, 'Network.loadingFinished')
    .then(([event]) => verifyLoadingFinished(event));

  await fetch(url, {
    method: 'POST',
    body: 'foobar',
    headers: requestHeaders,
  });

  await requestWillBeSentFuture;
  const responseReceived = await responseReceivedFuture;
  const loadingFinished = await loadingFinishedFuture;
  assert.ok(loadingFinished.timestamp >= responseReceived.timestamp);

  const requestBody = await session.post('Network.getRequestPostData', {
    requestId: responseReceived.requestId,
  });
  assert.strictEqual(requestBody.postData, 'foobar');

  const responseBody = await session.post('Network.getResponseBody', {
    requestId: responseReceived.requestId,
  });
  assert.strictEqual(responseBody.base64Encoded, false);
  assert.strictEqual(responseBody.body, 'hello world\n');
}

async function testRequestError(url) {
  const requestWillBeSentFuture = once(session, 'Network.requestWillBeSent')
    .then(([event]) => verifyRequestWillBeSent(event, { url, method: 'GET' }));
  session.on('Network.responseReceived', common.mustNotCall());
  session.on('Network.loadingFinished', common.mustNotCall());

  const loadingFailedFuture = once(session, 'Network.loadingFailed')
    .then(([event]) => verifyLoadingFailed(event));

  fetch(url, {
    headers: requestHeaders,
  }).catch(common.mustCall());

  await requestWillBeSentFuture;
  await loadingFailedFuture;
}

const testNetworkInspection = async () => {
  // HTTP
  await testRequest(`http://127.0.0.1:${httpServer.address().port}/hello-world`);
  session.removeAllListeners();
  // HTTPS
  await testRequest(`https://127.0.0.1:${httpsServer.address().port}/hello-world`);
  session.removeAllListeners();
  // HTTP with invalid host
  await testRequestError(`http://${addresses.INVALID_HOST}/`);
  session.removeAllListeners();
  // HTTPS with invalid host
  await testRequestError(`https://${addresses.INVALID_HOST}/`);
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
