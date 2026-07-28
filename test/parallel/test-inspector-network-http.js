// Flags: --inspect=0 --experimental-network-inspection
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

const session = new inspector.Session();
session.connect();

const requestHeaders = {
  'accept-language': 'en-US',
  'Cookie': ['k1=v1', 'k2=v2'],
  'age': 1000,
  'x-header1': ['value1', 'value2']
};

const setResponseHeaders = (res) => {
  res.setHeader('server', 'node');
  res.setHeader('etag', 12345);
  res.setHeader('Set-Cookie', ['key1=value1', 'key2=value2']);
  res.setHeader('x-header2', ['value1', 'value2']);
  res.setHeader('Content-Type', 'text/plain; charset=utf-8');
};

const kTimeout = 1000;
const kDelta = 200;
const kDefaultResponseHeaders = {
  'server': 'node',
  'etag': '12345',
  'set-cookie': 'key1=value1\nkey2=value2',
  'x-header2': 'value1, value2',
};

function getDefaultResponseExpect(url) {
  return {
    url,
    mimeType: 'text/plain',
    charset: 'utf-8',
    responseHeaders: kDefaultResponseHeaders,
  };
}

function getPathName(req) {
  return new URL(req.url, `http://${req.headers.host}`).pathname;
}

const handleRequest = (req, res) => {
  const path = getPathName(req);
  switch (path) {
    case '/hello-world':
      setResponseHeaders(res);
      res.writeHead(200);
      // Ensure the header is sent.
      res.write('\n');

      setTimeout(() => {
        res.end('hello world\n');
      }, kTimeout);
      break;
    case '/echo-post': {
      const chunks = [];
      req.on('data', (chunk) => {
        chunks.push(chunk);
      });
      req.on('end', () => {
        const body = Buffer.concat(chunks).toString();
        res.setHeader('Content-Type', 'application/json; charset=utf-8');
        res.writeHead(200);
        res.end(JSON.stringify({
          method: req.method,
          body,
        }));
      });
      break;
    }
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
  assert.strictEqual(params.request.method, expect.method ?? 'GET');
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
  assert.strictEqual(params.type, 'Other');
  assert.strictEqual(params.response.status, 200);
  assert.strictEqual(params.response.statusText, 'OK');
  assert.strictEqual(params.response.url, expect.url);
  assert.strictEqual(typeof params.response.headers, 'object');
  if (expect.responseHeaders?.server) {
    assert.strictEqual(params.response.headers.server, expect.responseHeaders.server);
  }
  if (expect.responseHeaders?.etag) {
    assert.strictEqual(params.response.headers.etag, expect.responseHeaders.etag);
  }
  if (expect.responseHeaders?.['set-cookie']) {
    assert.strictEqual(params.response.headers['set-cookie'], expect.responseHeaders['set-cookie']);
  }
  if (expect.responseHeaders?.['x-header2']) {
    assert.strictEqual(params.response.headers['x-header2'], expect.responseHeaders['x-header2']);
  }
  assert.strictEqual(params.response.mimeType, expect.mimeType);
  assert.strictEqual(params.response.charset, expect.charset);

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
  assert.strictEqual(params.type, 'Other');
  assert.strictEqual(typeof params.errorText, 'string');
}

function verifyHttpResponse(response) {
  assert.strictEqual(response.statusCode, 200);
  const chunks = [];

  // Verifies that the inspector does not put the response into flowing mode.
  assert.strictEqual(response.readableFlowing, null);
  // Verifies that the data listener may be added at a later time, and it can
  // still observe the data in full.
  queueMicrotask(common.mustCall(() => {
    response.on('data', (chunk) => {
      chunks.push(chunk);
    });
    assert.strictEqual(response.readableFlowing, true);
  }));

  response.on('end', common.mustCall(() => {
    const body = Buffer.concat(chunks).toString();
    assert.strictEqual(body, '\nhello world\n');
  }));
}

function drainHttpResponse(response) {
  response.resume();
}

function createRequestTracker(url, responseExpect, requestExpect = {}) {
  const requestWillBeSentFuture = once(session, 'Network.requestWillBeSent')
    .then(([event]) => verifyRequestWillBeSent(event, {
      url,
      method: requestExpect.method,
    }));

  const responseReceivedFuture = once(session, 'Network.responseReceived')
    .then(([event]) => verifyResponseReceived(event, responseExpect));

  const loadingFinishedFuture = once(session, 'Network.loadingFinished')
    .then(([event]) => verifyLoadingFinished(event));

  return {
    requestWillBeSentFuture,
    responseReceivedFuture,
    loadingFinishedFuture,
  };
}

async function assertResponseBody(responseReceived, expectedBody, expectedBase64Encoded = false) {
  const responseBody = await session.post('Network.getResponseBody', {
    requestId: responseReceived.requestId,
  });
  assert.strictEqual(responseBody.base64Encoded, expectedBase64Encoded);
  assert.strictEqual(responseBody.body, expectedBody);
}

async function testHttpGet() {
  const url = `http://127.0.0.1:${httpServer.address().port}/hello-world`;
  const {
    requestWillBeSentFuture,
    responseReceivedFuture,
    loadingFinishedFuture,
  } = createRequestTracker(url, getDefaultResponseExpect(url));

  http.get({
    host: '127.0.0.1',
    port: httpServer.address().port,
    path: '/hello-world',
    headers: requestHeaders
  }, common.mustCall(verifyHttpResponse));

  await requestWillBeSentFuture;
  const responseReceived = await responseReceivedFuture;
  const loadingFinished = await loadingFinishedFuture;

  const delta = (loadingFinished.timestamp - responseReceived.timestamp) * 1000;
  assert.ok(delta > kDelta);
  await assertResponseBody(responseReceived, '\nhello world\n');
}

async function testHttpGetWithAbsoluteUrlPath() {
  const url = `http://127.0.0.1:${httpServer.address().port}/hello-world`;
  const {
    requestWillBeSentFuture,
    responseReceivedFuture,
    loadingFinishedFuture,
  } = createRequestTracker(url, getDefaultResponseExpect(url));

  http.get({
    host: '127.0.0.1',
    port: httpServer.address().port,
    path: url,
    headers: requestHeaders,
  }, common.mustCall(verifyHttpResponse));

  await requestWillBeSentFuture;
  const responseReceived = await responseReceivedFuture;
  const loadingFinished = await loadingFinishedFuture;

  const delta = (loadingFinished.timestamp - responseReceived.timestamp) * 1000;
  assert.ok(delta > kDelta);
  await assertResponseBody(responseReceived, '\nhello world\n');
}

async function testHttpPostWithAbsoluteUrlPath() {
  const requestBody = JSON.stringify({ title: 'foo', type: 'post' });
  const url = `http://127.0.0.1:${httpServer.address().port}/echo-post`;
  const {
    requestWillBeSentFuture,
    responseReceivedFuture,
    loadingFinishedFuture,
  } = createRequestTracker(url, {
    url,
    mimeType: 'application/json',
    charset: 'utf-8',
  }, {
    method: 'POST',
  });

  const responsePromise = new Promise((resolve, reject) => {
    const req = http.request({
      host: '127.0.0.1',
      port: httpServer.address().port,
      path: url,
      method: 'POST',
      headers: {
        ...requestHeaders,
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(requestBody),
      },
    }, resolve);
    req.on('error', reject);
    req.end(requestBody);
  });

  const response = await responsePromise;
  drainHttpResponse(response);

  await requestWillBeSentFuture;
  const responseReceived = await responseReceivedFuture;
  await loadingFinishedFuture;
  await assertResponseBody(responseReceived, JSON.stringify({
    method: 'POST',
    body: requestBody,
  }));
}

async function testHttpsGet() {
  const url = `https://127.0.0.1:${httpsServer.address().port}/hello-world`;
  const {
    requestWillBeSentFuture,
    responseReceivedFuture,
    loadingFinishedFuture,
  } = createRequestTracker(url, getDefaultResponseExpect(url));

  https.get({
    host: '127.0.0.1',
    port: httpsServer.address().port,
    path: '/hello-world',
    rejectUnauthorized: false,
    headers: requestHeaders,
  }, common.mustCall(verifyHttpResponse));

  await requestWillBeSentFuture;
  const responseReceived = await responseReceivedFuture;
  const loadingFinished = await loadingFinishedFuture;

  const delta = (loadingFinished.timestamp - responseReceived.timestamp) * 1000;
  assert.ok(delta > kDelta);
  await assertResponseBody(responseReceived, '\nhello world\n');
}

async function testHttpError() {
  const url = `http://${addresses.INVALID_HOST}/`;
  const requestWillBeSentFuture = once(session, 'Network.requestWillBeSent')
    .then(([event]) => verifyRequestWillBeSent(event, { url }));
  session.on('Network.responseReceived', common.mustNotCall());
  session.on('Network.loadingFinished', common.mustNotCall());

  const loadingFailedFuture = once(session, 'Network.loadingFailed')
    .then(([event]) => verifyLoadingFailed(event));

  http.get({
    host: addresses.INVALID_HOST,
    headers: requestHeaders,
  }, common.mustNotCall()).on('error', common.mustCall());

  await requestWillBeSentFuture;
  await loadingFailedFuture;
}

async function testHttpsError() {
  const url = `https://${addresses.INVALID_HOST}/`;
  const requestWillBeSentFuture = once(session, 'Network.requestWillBeSent')
    .then(([event]) => verifyRequestWillBeSent(event, { url }));
  session.on('Network.responseReceived', common.mustNotCall());
  session.on('Network.loadingFinished', common.mustNotCall());

  const loadingFailedFuture = once(session, 'Network.loadingFailed')
    .then(([event]) => verifyLoadingFailed(event));

  https.get({
    host: addresses.INVALID_HOST,
    headers: requestHeaders,
  }, common.mustNotCall()).on('error', common.mustCall());

  await requestWillBeSentFuture;
  await loadingFailedFuture;
}

const testNetworkInspection = async () => {
  await testHttpGet();
  session.removeAllListeners();
  await testHttpGetWithAbsoluteUrlPath();
  session.removeAllListeners();
  await testHttpPostWithAbsoluteUrlPath();
  session.removeAllListeners();
  await testHttpsGet();
  session.removeAllListeners();
  await testHttpError();
  session.removeAllListeners();
  await testHttpsError();
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
