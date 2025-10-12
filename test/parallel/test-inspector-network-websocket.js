// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
common.skipIfInspectorDisabled();

const assert = require('node:assert');
const { once } = require('node:events');
const WebSocketServer = require('../common/websocket-server');
const inspector = require('node:inspector/promises');
const dc = require('diagnostics_channel');

const nameRE = 'undici' in process.versions ? /^node:internal\/deps\/undici\/undici$/u : /undici/u;

const session = new inspector.Session();
session.connect();

dc.channel('undici:websocket:socket_error').subscribe((message) => {
  console.error('WebSocket error:', message);
});

function findFrameInInitiator(regex, initiator) {
  const frame = initiator.stack.callFrames.find((it) => {
    return regex.test(it.url);
  });
  return frame;
}

async function test() {
  await session.post('Network.enable');
  const server = new WebSocketServer({
    responseError: true,
  });
  await server.start();
  const url = `ws://127.0.0.1:${server.port}/`;
  let requestId;
  once(session, 'Network.webSocketCreated').then(common.mustCall(([message]) => {
    assert.strictEqual(message.method, 'Network.webSocketCreated');
    assert.strictEqual(message.params.url, url);
    assert.ok(message.params.requestId);
    assert.strictEqual(typeof message.params.initiator, 'object');
    assert.strictEqual(message.params.initiator.type, 'script');
    assert.ok(findFrameInInitiator(nameRE, message.params.initiator));
    requestId = message.params.requestId;
  }));

  once(session, 'Network.webSocketHandshakeResponseReceived').then(common.mustCall(([message]) => {
    assert.strictEqual(message.params.requestId, requestId);
    assert.strictEqual(message.params.response.status, 101);
    assert.strictEqual(message.params.response.statusText, 'Switching Protocols');
    assert.strictEqual(typeof message.params.timestamp, 'number');
    socket.close();
  }));

  const socket = new WebSocket(url);
  once(session, 'Network.webSocketClosed').then(common.mustCall(([message]) => {
    assert.strictEqual(message.method, 'Network.webSocketClosed');
    assert.strictEqual(message.params.requestId, requestId);
    session.disconnect();
    server.server.close();
  }));
}

test().then(common.mustCall());
