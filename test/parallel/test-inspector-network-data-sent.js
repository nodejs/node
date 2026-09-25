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

async function triggerNetworkEvents(requestId, requestCharset) {
  const url = 'https://example.com';
  Network.requestWillBeSent({
    requestId,
    timestamp: 1,
    wallTime: 1,
    request: {
      url,
      method: 'PUT',
      headers: {
        mKey: 'mValue',
      },
      hasPostData: true,
    },
    charset: requestCharset,
  });
  await setTimeout(1);

  const chunk1 = Buffer.from('Hello, ');
  Network.dataSent({
    requestId,
    timestamp: 2,
    dataLength: chunk1.byteLength,
    data: chunk1,
  });
  await setTimeout(1);

  const chunk2 = Buffer.from('world');
  Network.dataSent({
    requestId,
    timestamp: 3,
    dataLength: chunk2.byteLength,
    data: chunk2,
  });
  await setTimeout(1);

  Network.dataSent({
    requestId,
    finished: true,
  });
  await setTimeout(1);

  Network.responseReceived({
    requestId,
    timestamp: 4,
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

  Network.loadingFinished({
    requestId,
    timestamp: 5,
  });
}

function assertNetworkEvents(session, requestId) {
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

  return loadingFinishedFuture;
}

test('Network.getRequestPostData should send all buffered text data', async () => {
  session.removeAllListeners();

  const requestId = 'my-req-id-1';
  const loadingFinishedFuture = assertNetworkEvents(session, requestId);

  await triggerNetworkEvents(requestId, 'utf-8');
  await loadingFinishedFuture;

  const { postData } = await session.post('Network.getRequestPostData', {
    requestId,
  });
  assert.strictEqual(postData, 'Hello, world');
});

test('Network.getRequestPostData does not support binary data', async () => {
  session.removeAllListeners();

  const requestId = 'my-req-id-2';
  const loadingFinishedFuture = assertNetworkEvents(session, requestId);

  await triggerNetworkEvents(requestId);
  await loadingFinishedFuture;

  await assert.rejects(session.post('Network.getRequestPostData', {
    requestId,
  }), {
    code: 'ERR_INSPECTOR_COMMAND',
  });
});

test('Network.getRequestPostData should reject if request id not found', async () => {
  session.removeAllListeners();

  const requestId = 'unknown-request-id';
  await assert.rejects(session.post('Network.getRequestPostData', {
    requestId,
  }), {
    code: 'ERR_INSPECTOR_COMMAND',
  });
});
