// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
common.skipIfInspectorDisabled();

const assert = require('node:assert');
const fixtures = require('../common/fixtures');
const { on, once } = require('node:events');
const http2 = require('node:http2');
const inspector = require('node:inspector/promises');

const session = new inspector.Session();
session.connect();

const requestHeaders = {
  'x-header1': ['value1', 'value2'],
  [http2.constants.HTTP2_HEADER_ACCEPT_LANGUAGE]: 'en-US',
  [http2.constants.HTTP2_HEADER_AGE]: 1000,
  [http2.constants.HTTP2_HEADER_COOKIE]: ['k1=v1', 'k2=v2'],
  [http2.constants.HTTP2_HEADER_METHOD]: 'GET',
  [http2.constants.HTTP2_HEADER_PATH]: '/hello-world',
};

const requestErrorHeaders = {
  'x-header1': ['value1', 'value2'],
  [http2.constants.HTTP2_HEADER_ACCEPT_LANGUAGE]: 'en-US',
  [http2.constants.HTTP2_HEADER_AGE]: 1000,
  [http2.constants.HTTP2_HEADER_COOKIE]: ['k1=v1', 'k2=v2'],
  [http2.constants.HTTP2_HEADER_METHOD]: 'GET',
  [http2.constants.HTTP2_HEADER_PATH]: '/trigger-error',
};

const responseHeaders = {
  'x-header2': ['value1', 'value2'],
  [http2.constants.HTTP2_HEADER_CONTENT_TYPE]: 'text/plain; charset=utf-8',
  [http2.constants.HTTP2_HEADER_ETAG]: 12345,
  [http2.constants.HTTP2_HEADER_SERVER]: 'node',
  [http2.constants.HTTP2_HEADER_SET_COOKIE]: ['key1=value1', 'key2=value2'],
  [http2.constants.HTTP2_HEADER_STATUS]: 200,
};

const pushRequestHeaders = {
  'x-header3': ['value1', 'value2'],
  'x-push': 'true',
  [http2.constants.HTTP2_HEADER_PATH]: '/style.css',
};

const pushResponseHeaders = {
  'x-header4': ['value1', 'value2'],
  'x-push': 'true',
  [http2.constants.HTTP2_HEADER_CONTENT_TYPE]: 'text/css',
  [http2.constants.HTTP2_HEADER_STATUS]: 200,
};

const kTimeout = 1000;
const kDelta = 200;

const handleStream = (stream, headers) => {
  const path = headers[http2.constants.HTTP2_HEADER_PATH];
  switch (path) {
    case '/hello-world':
      stream.pushStream(pushRequestHeaders, common.mustSucceed((pushStream) => {
        pushStream.respond(pushResponseHeaders);
        pushStream.end('body { color: red; }\n');
      }));

      stream.respond(responseHeaders);

      setTimeout(() => {
        stream.end('hello world\n');
      }, kTimeout);
      break;
    case '/trigger-error':
      stream.close(http2.constants.NGHTTP2_STREAM_CLOSED);
      stream.on('error', common.expectsError({
        code: 'ERR_HTTP2_STREAM_ERROR',
        name: 'Error',
        message: 'Stream closed with error code NGHTTP2_STREAM_CLOSED'
      }));
      break;
    default:
      assert(false, `Unexpected path: ${path}`);
  }
};

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

function findFrameInInitiator(scriptName, initiator) {
  const frame = initiator.stack.callFrames.find((it) => {
    return it.url === scriptName;
  });
  return frame;
}

function verifyRequestWillBeSent({ method, params }, expectedUrl) {
  assert.strictEqual(method, 'Network.requestWillBeSent');

  assert.ok(params.requestId.startsWith('node-network-event-'));
  assert.strictEqual(params.request.url, expectedUrl);
  assert.strictEqual(params.request.method, 'GET');
  assert.strictEqual(typeof params.request.headers, 'object');

  if (expectedUrl.endsWith('/hello-world')) {
    assert.strictEqual(params.request.headers['accept-language'], 'en-US');
    assert.strictEqual(params.request.headers.cookie, 'k1=v1; k2=v2');
    assert.strictEqual(params.request.headers.age, '1000');
    assert.strictEqual(params.request.headers['x-header1'], 'value1, value2');
    assert.ok(findFrameInInitiator(__filename, params.initiator));
  } else if (expectedUrl.endsWith('/style.css')) {
    assert.strictEqual(params.request.headers['x-header3'], 'value1, value2');
    assert.strictEqual(params.request.headers['x-push'], 'true');
    assert.ok(!findFrameInInitiator(__filename, params.initiator));
  }

  assert.strictEqual(typeof params.timestamp, 'number');
  assert.strictEqual(typeof params.wallTime, 'number');

  assert.strictEqual(typeof params.initiator, 'object');
  assert.strictEqual(params.initiator.type, 'script');

  return params;
}

function verifyResponseReceived({ method, params }, expectedUrl) {
  assert.strictEqual(method, 'Network.responseReceived');

  assert.ok(params.requestId.startsWith('node-network-event-'));
  assert.strictEqual(typeof params.timestamp, 'number');
  assert.strictEqual(params.type, 'Other');
  assert.strictEqual(params.response.status, 200);
  assert.strictEqual(params.response.statusText, '');
  assert.strictEqual(params.response.url, expectedUrl);
  assert.strictEqual(typeof params.response.headers, 'object');
  if (expectedUrl.endsWith('/hello-world')) {
    assert.strictEqual(params.response.headers.server, 'node');
    assert.strictEqual(params.response.headers.etag, '12345');
    assert.strictEqual(params.response.headers['set-cookie'], 'key1=value1\nkey2=value2');
    assert.strictEqual(params.response.headers['x-header2'], 'value1, value2');
    assert.strictEqual(params.response.mimeType, 'text/plain');
    assert.strictEqual(params.response.charset, 'utf-8');
  } else if (expectedUrl.endsWith('/style.css')) {
    assert.strictEqual(params.response.headers['x-header4'], 'value1, value2');
    assert.strictEqual(params.response.headers['x-push'], 'true');
    assert.strictEqual(params.response.mimeType, 'text/css');
    assert.strictEqual(params.response.charset, '');
  }

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

async function testHttp2(secure = false) {
  const port = (secure ? http2SecureServer : http2Server).address().port;
  const origin = (secure ? 'https' : 'http') + `://localhost:${port}`;
  const url = `${origin}/hello-world`;
  const pushedUrl = `${origin}/style.css`;

  const requestWillBeSent = on(session, 'Network.requestWillBeSent');
  const responseReceived = on(session, 'Network.responseReceived');
  const loadingFinished = on(session, 'Network.loadingFinished');

  session.on('Network.loadingFailed', common.mustNotCall());

  const client = http2.connect(origin, {
    rejectUnauthorized: false,
  });
  const request = client.request(requestHeaders);

  // Dump the responses.
  request.on('data', () => {});
  client.on('stream', (pushStream) => {
    pushStream.on('data', () => {});
  });
  request.on('end', () => {
    client.close();
  });
  request.end();

  const [
    { value: [ mainRequest ] },
    { value: [ pushRequest ] },
  ] = await Promise.all([requestWillBeSent.next(), requestWillBeSent.next()]);
  verifyRequestWillBeSent(mainRequest, url);
  verifyRequestWillBeSent(pushRequest, pushedUrl);

  const [
    { value: [ mainResponse ] },
    { value: [ pushResponse ] },
  ] = await Promise.all([responseReceived.next(), responseReceived.next()]);
  verifyResponseReceived(mainResponse, url);
  verifyResponseReceived(pushResponse, pushedUrl);

  const [
    { value: [ event1 ] },
    { value: [ event2 ] },
  ] = await Promise.all([loadingFinished.next(), loadingFinished.next()]);
  verifyLoadingFinished(event1);
  verifyLoadingFinished(event2);

  const mainFinished = [event1, event2]
    .find((event) => event.params.requestId === mainResponse.params.requestId);
  const pushFinished = [event1, event2]
    .find((event) => event.params.requestId === pushResponse.params.requestId);

  assert.ok(mainFinished.params.timestamp >= mainResponse.params.timestamp);
  assert.ok(pushFinished.params.timestamp >= pushResponse.params.timestamp);

  const delta =
    (mainFinished.params.timestamp - mainResponse.params.timestamp) * 1000;
  assert.ok(delta > kDelta);
}

async function testHttp2Error(secure = false) {
  const port = (secure ? http2SecureServer : http2Server).address().port;
  const origin = (secure ? 'https' : 'http') + `://localhost:${port}`;
  const errorUrl = `${origin}/trigger-error`;

  const requestWillBeSent = once(session, 'Network.requestWillBeSent');
  session.on('Network.responseReceived', common.mustNotCall());
  session.on('Network.loadingFinished', common.mustNotCall());
  const loadingFailed = once(session, 'Network.loadingFailed');

  const client = http2.connect(origin, {
    rejectUnauthorized: false,
  });
  const request = client.request(requestErrorHeaders);

  request.on('close', common.mustCall(() => {
    assert.strictEqual(request.rstCode, http2.constants.NGHTTP2_STREAM_CLOSED);
    client.close();
  }));
  request.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    name: 'Error',
    message: 'Stream closed with error code NGHTTP2_STREAM_CLOSED'
  }));
  request.end();

  const [ requestEvent ] = await requestWillBeSent;
  verifyRequestWillBeSent(requestEvent, errorUrl);

  const [ failed ] = await loadingFailed;
  verifyLoadingFailed(failed);
}

const testNetworkInspection = async () => {
  await testHttp2();
  session.removeAllListeners();
  await testHttp2(true);
  session.removeAllListeners();
  await testHttp2Error();
  session.removeAllListeners();
  await testHttp2Error(true);
  session.removeAllListeners();
};

http2Server.listen(0, async () => {
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
