// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const inspector = require('node:inspector/promises');
const { Network } = require('node:inspector');
const test = require('node:test');
const assert = require('node:assert');
const { waitUntil } = require('../common/inspector-helper');
const { setTimeout } = require('node:timers/promises');

const session = new inspector.Session();
session.connect();
session.post('Network.enable');

async function triggerNetworkEvents(requestId) {
  const url = 'https://example.com';
  Network.requestWillBeSent({
    requestId,
    timestamp: 1,
    wallTime: 1,
    request: {
      url,
      method: 'GET',
      headers: {
        mKey: 'mValue',
      },
    },
  });
  await setTimeout(1);

  Network.responseReceived({
    requestId,
    timestamp: 2,
    type: 'Fetch',
    response: {
      url,
      status: 200,
      statusText: 'OK',
      headers: {
        mKey: 'mValue',
      },
    },
  });
  await setTimeout(1);

  const chunk1 = Buffer.from('Hello, ');
  Network.dataReceived({
    requestId,
    timestamp: 3,
    dataLength: chunk1.byteLength,
    encodedDataLength: chunk1.byteLength,
    data: chunk1,
  });
  await setTimeout(1);

  const chunk2 = Buffer.from('world');
  Network.dataReceived({
    requestId,
    timestamp: 4,
    dataLength: chunk2.byteLength,
    encodedDataLength: chunk2.byteLength,
    data: chunk2,
  });
  await setTimeout(1);

  Network.loadingFinished({
    requestId,
    timestamp: 5,
  });
}

test('should stream Network.dataReceived with data chunks', async () => {
  session.removeAllListeners();

  const requestId = 'my-req-id-1';
  const chunks = [];
  let totalDataLength = 0;
  session.on('Network.requestWillBeSent', common.mustCall(({ params }) => {
    assert.strictEqual(params.requestId, requestId);
  }));
  const responseReceivedFuture = waitUntil(session, 'Network.responseReceived')
    .then(async ([{ params }]) => {
      assert.strictEqual(params.requestId, requestId);
      const { bufferedData } = await session.post('Network.streamResourceContent', {
        requestId,
      });
      const data = Buffer.from(bufferedData, 'base64');
      totalDataLength += data.byteLength;
      chunks.push(data);
    });
  const loadingFinishedFuture = waitUntil(session, 'Network.loadingFinished')
    .then(([{ params }]) => {
      assert.strictEqual(params.requestId, requestId);
    });
  session.on('Network.dataReceived', ({ params }) => {
    assert.strictEqual(params.requestId, requestId);
    totalDataLength += params.dataLength;
    chunks.push(Buffer.from(params.data, 'base64'));
  });

  await triggerNetworkEvents(requestId);
  await responseReceivedFuture;
  await loadingFinishedFuture;

  const data = Buffer.concat(chunks);
  assert.strictEqual(data.byteLength, totalDataLength, data);
  assert.strictEqual(data.toString('utf8'), 'Hello, world');
});

test('Network.streamResourceContent should send all buffered chunks', async () => {
  session.removeAllListeners();

  const requestId = 'my-req-id-2';
  session.on('Network.requestWillBeSent', common.mustCall(({ params }) => {
    assert.strictEqual(params.requestId, requestId);
  }));
  session.on('Network.responseReceived', common.mustCall(({ params }) => {
    assert.strictEqual(params.requestId, requestId);
  }));
  const loadingFinishedFuture = waitUntil(session, 'Network.loadingFinished')
    .then(async ([{ params }]) => {
      assert.strictEqual(params.requestId, requestId);
    });
  session.on('Network.dataReceived', common.mustNotCall());

  await triggerNetworkEvents(requestId);
  await loadingFinishedFuture;
  const { bufferedData } = await session.post('Network.streamResourceContent', {
    requestId,
  });
  assert.strictEqual(Buffer.from(bufferedData, 'base64').toString('utf8'), 'Hello, world');
});

test('Network.streamResourceContent should reject if request id not found', async () => {
  session.removeAllListeners();

  const requestId = 'unknown-request-id';
  await assert.rejects(session.post('Network.streamResourceContent', {
    requestId,
  }), {
    code: 'ERR_INSPECTOR_COMMAND',
  });
});
