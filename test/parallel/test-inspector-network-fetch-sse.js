// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('node:assert');
const { once } = require('node:events');
const http = require('node:http');
const inspector = require('node:inspector/promises');
const { setTimeout: delay } = require('node:timers/promises');
const zlib = require('node:zlib');

const session = new inspector.Session();
session.connect();

const plainChunks = [
  'id: alpha\r',
  '\n',
  'data: line 1\r',
  '\n',
  'data: line 2\r\n',
  '\r\n',
  'event: custom\r\n',
  'data: chunked',
  ' message\r\n',
  '\r\n',
  'id: beta\r\n',
  'data: second\r\n',
  '\r\n',
  'data: inherited\r\n\r\n',
];

const plainExpectedMessages = [
  {
    eventName: 'message',
    eventId: 'alpha',
    data: 'line 1\nline 2',
  },
  {
    eventName: 'custom',
    eventId: 'alpha',
    data: 'chunked message',
  },
  {
    eventName: 'message',
    eventId: 'beta',
    data: 'second',
  },
  {
    eventName: 'message',
    eventId: 'beta',
    data: 'inherited',
  },
];

const compressedBody = [
  'id: zipped',
  'data: compressed line 1',
  'data: compressed line 2',
  '',
  'data: inherited zipped',
  '',
  '',
].join('\n');

const compressedExpectedMessages = [
  {
    eventName: 'message',
    eventId: 'zipped',
    data: 'compressed line 1\ncompressed line 2',
  },
  {
    eventName: 'message',
    eventId: 'zipped',
    data: 'inherited zipped',
  },
];

async function writeChunks(res, chunks) {
  for (const chunk of chunks) {
    res.write(chunk);
    await delay(5);
  }
  res.end();
}

async function writeBufferInChunks(res, buffer) {
  const midpoint = Math.max(1, Math.floor(buffer.length / 2));
  res.write(buffer.subarray(0, midpoint));
  await delay(5);
  res.end(buffer.subarray(midpoint));
}

const server = http.createServer((req, res) => {
  switch (req.url) {
    case '/plain':
      res.setHeader('Content-Type', 'text/event-stream; charset=utf-8');
      res.writeHead(200);
      void writeChunks(res, plainChunks);
      break;
    case '/invalid-type':
      res.setHeader('Content-Type', 'text/plain; charset=utf-8');
      res.writeHead(200);
      void writeChunks(res, plainChunks);
      break;
    case '/gzip':
      res.setHeader('Content-Type', 'text/event-stream; charset=utf-8');
      res.setHeader('Content-Encoding', 'gzip');
      res.writeHead(200);
      void writeBufferInChunks(res, zlib.gzipSync(compressedBody));
      break;
    case '/deflate':
      res.setHeader('Content-Type', 'text/event-stream; charset=utf-8');
      res.setHeader('Content-Encoding', 'deflate');
      res.writeHead(200);
      void writeBufferInChunks(res, zlib.deflateSync(compressedBody));
      break;
    default:
      assert.fail(`Unexpected path: ${req.url}`);
  }
});

function observeSseRequest(url, expectedMessages) {
  return new Promise((resolve, reject) => {
    let requestId;
    const received = [];
    const timer = setTimeout(() => {
      cleanup();
      reject(new Error(`Timed out waiting for SSE messages for ${url}: received ${received.length}`));
    }, 5000);

    function cleanup() {
      clearTimeout(timer);
      session.off('Network.responseReceived', onResponseReceived);
      session.off('Network.eventSourceMessageReceived', onEventSourceMessageReceived);
    }

    function maybeResolve() {
      if (requestId === undefined || received.length !== expectedMessages.length) {
        return;
      }

      try {
        for (const message of received) {
          assert.strictEqual(message.requestId, requestId);
          assert.strictEqual(typeof message.timestamp, 'number');
        }
        assert.deepStrictEqual(
          received.map(({ eventName, eventId, data }) => ({ eventName, eventId, data })),
          expectedMessages,
        );
        cleanup();
        resolve(requestId);
      } catch (error) {
        cleanup();
        reject(error);
      }
    }

    function onResponseReceived({ params }) {
      if (params.response.url !== url) {
        return;
      }

      try {
        assert.strictEqual(params.type, 'EventSource');
        assert.strictEqual(params.response.mimeType, 'text/event-stream');
        assert.strictEqual(params.response.charset, 'utf-8');
        requestId = params.requestId;
        maybeResolve();
      } catch (error) {
        cleanup();
        reject(error);
      }
    }

    function onEventSourceMessageReceived({ params }) {
      if (requestId !== undefined && params.requestId !== requestId) {
        return;
      }
      received.push(params);
      maybeResolve();
    }

    session.on('Network.responseReceived', onResponseReceived);
    session.on('Network.eventSourceMessageReceived', onEventSourceMessageReceived);
  });
}

async function expectNoSseMessages(url, fetchImpl) {
  const messages = [];
  const onEventSourceMessageReceived = ({ params }) => {
    messages.push(params);
  };
  session.on('Network.eventSourceMessageReceived', onEventSourceMessageReceived);

  try {
    const responseReceivedPromise = once(session, 'Network.responseReceived')
      .then(([{ params }]) => {
        assert.strictEqual(params.response.url, url);
        assert.strictEqual(params.type, 'Fetch');
        assert.strictEqual(params.response.mimeType, 'text/plain');
        return params.requestId;
      });

    const response = await fetchImpl(url);
    await response.text();
    const requestId = await responseReceivedPromise;
    await delay(50);

    assert.deepStrictEqual(
      messages.filter((message) => message.requestId === requestId),
      [],
    );
  } finally {
    session.off('Network.eventSourceMessageReceived', onEventSourceMessageReceived);
  }
}

async function expectSseMessages(url, fetchImpl, expectedMessages) {
  const observer = observeSseRequest(url, expectedMessages);
  const response = await fetchImpl(url);
  assert.strictEqual(response.status, 200);
  await response.text();
  await observer;
}

(async () => {
  server.listen(0);
  await once(server, 'listening');

  const baseUrl = `http://127.0.0.1:${server.address().port}`;

  try {
    await session.post('Network.enable');

    await expectSseMessages(`${baseUrl}/plain`, fetch, plainExpectedMessages);
    await expectSseMessages(`${baseUrl}/gzip`, fetch, compressedExpectedMessages);
    await expectSseMessages(`${baseUrl}/deflate`, fetch, compressedExpectedMessages);
    await expectNoSseMessages(`${baseUrl}/invalid-type`, fetch);

    await session.post('Network.disable');
  } catch (error) {
    assert.fail(error);
  } finally {
    session.disconnect();
    await new Promise((resolve) => server.close(resolve));
    await inspector.close();
  }
})().then(common.mustCall());
