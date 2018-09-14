// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

// Test inspector waitForConnection() API. It uses ephemeral ports so can be
// run safely in parallel.

const assert = require('assert');
const fork = require('child_process').fork;
const http = require('http');
const url = require('url');

if (process.env.BE_CHILD)
  return beChild();

const { formatWSFrame } = require('../common/inspector-helper.js');

function waitForMessage(child) {
  return new Promise(child.once.bind(child, 'message'));
}

async function connect(wsUrl) {
  const socket = await new Promise((resolve, reject) => {
    const parsedUrl = url.parse(wsUrl);
    const response = http.get({
      port: parsedUrl.port,
      path: parsedUrl.path,
      headers: {
        'Connection': 'Upgrade',
        'Upgrade': 'websocket',
        'Sec-WebSocket-Version': 13,
        'Sec-WebSocket-Key': 'key=='
      }
    });
    response.once('upgrade', (message, socket) => resolve(socket));
    response.once('responce', () => reject('Upgrade was not received'));
  });
  return socket;
}

function runIfWaitingForDebugger(socket) {
  return new Promise((resolve) => socket.write(formatWSFrame({
    id: 1,
    method: 'Runtime.runIfWaitingForDebugger'
  }), resolve));
}

(async function() {
  const child = fork(__filename,
                     { env: Object.assign({}, process.env, { BE_CHILD: 1 }) });
  const started = await waitForMessage(child);
  assert.strictEqual(started.cmd, 'started');

  child.send({ cmd: 'open', args: [0] });
  const { url } = await waitForMessage(child);

  // Wait for connection first time..
  child.send({ cmd: 'waitForConnection' });
  // .. connect ..
  const socket = await connect(url);
  runIfWaitingForDebugger(socket);

  // .. check that waitForConnection method is finished.
  const awaited1 = await waitForMessage(child);
  assert.strictEqual(awaited1.cmd, 'awaited');

  // Wait for connection with existing connection ..
  child.send({ cmd: 'waitForConnection' });
  const awaited2 = await waitForMessage(child);
  // .. check that waitForConnection method is finished.
  assert.strictEqual(awaited2.cmd, 'awaited');

  socket.end();

  // Close connection ..
  child.send({ cmd: 'close' });
  const closed = await waitForMessage(child);
  assert.strictEqual(closed.cmd, 'closed');

  // .. call waitForConnection when inspector is closed ..
  child.send({ cmd: 'waitForConnection' });
  const awaitedError = await waitForMessage(child);
  // .. check error message.
  assert.strictEqual(awaitedError.cmd, 'awaited');
  assert.strictEqual(awaitedError.error,
                     'inspector error, inspector.open should be called first');

  child.send({ cmd: 'exit' });
})();

function beChild() {
  const inspector = require('inspector');

  process.send({ cmd: 'started' });

  process.on('message', (msg) => {
    if (msg.cmd === 'open') {
      inspector.open(...msg.args);
      process.send({ cmd: 'opened', url: inspector.url() });
    } else if (msg.cmd === 'close') {
      inspector.close();
      process.send({ cmd: 'closed' });
    } else if (msg.cmd === 'waitForConnection') {
      try {
        inspector.waitForConnection();
        process.send({ cmd: 'awaited' });
      } catch (e) {
        process.send({ cmd: 'awaited', error: e.message });
      }
    } else if (msg.cmd === 'exit') {
      process.exit();
    }
  });
}
