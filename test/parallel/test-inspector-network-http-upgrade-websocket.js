// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
common.skipIfInspectorDisabled();

const assert = require('node:assert');
const { once } = require('node:events');
const http = require('node:http');
const inspector = require('node:inspector/promises');
const WebSocketServer = require('../common/websocket-server');

function encodeFrame(opcode, payload, {
  masked = false,
  mask = Buffer.from([1, 2, 3, 4]),
} = {}) {
  payload = typeof payload === 'string' ? Buffer.from(payload) : payload;

  const header = [
    0x80 | opcode,
  ];

  if (payload.byteLength < 126) {
    header.push((masked ? 0x80 : 0) | payload.byteLength);
  } else {
    throw new Error('payload too large for test');
  }

  if (!masked) {
    return Buffer.concat([Buffer.from(header), payload]);
  }

  const maskedPayload = Buffer.from(payload);
  for (let i = 0; i < maskedPayload.byteLength; i++) {
    maskedPayload[i] ^= mask[i % 4];
  }

  return Buffer.concat([Buffer.from(header), mask, maskedPayload]);
}

const session = new inspector.Session();
session.connect();

async function test() {
  await session.post('Network.enable');

  const server = new WebSocketServer({});
  await server.start();

  const host = '127.0.0.1';
  const wsUrl = `ws://${host}:${server.port}/`;
  const httpEvents = {
    requestWillBeSent: [],
    responseReceived: [],
    loadingFinished: [],
  };

  session.on('Network.requestWillBeSent', (message) => {
    httpEvents.requestWillBeSent.push(message);
  });
  session.on('Network.responseReceived', (message) => {
    httpEvents.responseReceived.push(message);
  });
  session.on('Network.loadingFinished', (message) => {
    httpEvents.loadingFinished.push(message);
  });

  let requestId;
  const created = once(session, 'Network.webSocketCreated').then(common.mustCall(([message]) => {
    assert.strictEqual(message.method, 'Network.webSocketCreated');
    assert.strictEqual(message.params.url, wsUrl);
    assert.strictEqual(typeof message.params.requestId, 'string');
    requestId = message.params.requestId;
  }));

  const willSend = once(session, 'Network.webSocketWillSendHandshakeRequest').then(common.mustCall(([message]) => {
    assert.strictEqual(message.params.requestId, requestId);
    assert.strictEqual(typeof message.params.timestamp, 'number');
    assert.strictEqual(typeof message.params.wallTime, 'number');
    assert.strictEqual(message.params.request.headers.upgrade, 'websocket');
    assert.strictEqual(message.params.request.headers.connection, 'Upgrade');
    assert.strictEqual(message.params.request.headers['sec-websocket-version'], '13');
    assert.ok(message.params.request.headers['sec-websocket-key']);
  }));

  const handshake = once(session, 'Network.webSocketHandshakeResponseReceived').then(common.mustCall(([message]) => {
    assert.strictEqual(message.params.requestId, requestId);
    assert.strictEqual(message.params.response.status, 101);
    assert.strictEqual(message.params.response.statusText, 'Switching Protocols');
    assert.strictEqual(message.params.response.headers.upgrade, 'websocket');
    assert.strictEqual(message.params.response.headers.connection, 'Upgrade');
    assert.strictEqual(typeof message.params.timestamp, 'number');
  }));

  const frameSent = once(session, 'Network.webSocketFrameSent').then(common.mustCall(([message]) => {
    assert.strictEqual(message.params.requestId, requestId);
    assert.strictEqual(message.params.response.opcode, 1);
    assert.strictEqual(message.params.response.mask, true);
    assert.strictEqual(message.params.response.payloadData, 'hello');
    assert.strictEqual(typeof message.params.timestamp, 'number');
  }));

  const frameReceived = once(session, 'Network.webSocketFrameReceived').then(common.mustCall(([message]) => {
    assert.strictEqual(message.params.requestId, requestId);
    assert.strictEqual(message.params.response.opcode, 1);
    assert.strictEqual(message.params.response.mask, false);
    assert.strictEqual(message.params.response.payloadData, 'Hello from server!');
    assert.strictEqual(typeof message.params.timestamp, 'number');
  }));

  const closed = once(session, 'Network.webSocketClosed').then(common.mustCall(([message]) => {
    assert.strictEqual(message.method, 'Network.webSocketClosed');
    assert.strictEqual(message.params.requestId, requestId);
    assert.strictEqual(typeof message.params.timestamp, 'number');
  }));

  const req = http.get({
    host,
    port: server.port,
    headers: {
      'Connection': 'Upgrade',
      'Upgrade': 'websocket',
      'Sec-WebSocket-Key': 'dGhlIHNhbXBsZSBub25jZQ==',
      'Sec-WebSocket-Version': '13',
    },
  });
  req.on('error', common.mustNotCall());
  req.on('upgrade', common.mustCall((response, socket, head) => {
    assert.ok(head instanceof Buffer);
    assert.strictEqual(response.statusCode, 101);
    socket.on('error', common.mustNotCall());
    socket.resume();
    socket.once('data', common.mustCall(() => {
      socket.end(encodeFrame(0x8, Buffer.alloc(0), { masked: true }));
    }));
    socket.write(encodeFrame(0x1, 'hello', { masked: true }));
  }));

  await Promise.all([created, willSend, handshake, frameSent, frameReceived, closed]);
  assert.deepStrictEqual(httpEvents, {
    requestWillBeSent: [],
    responseReceived: [],
    loadingFinished: [],
  });

  session.disconnect();
  server.server.close();
}

test().then(common.mustCall());
