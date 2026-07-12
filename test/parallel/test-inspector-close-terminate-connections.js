'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { NodeInstance } = require('../common/inspector-helper.js');

// Verify that inspector.close() forcibly terminates active WebSocket
// connections without waiting for the client to disconnect.

async function test() {
  const script = `
    const inspector = require('inspector');
    inspector.close();
    process.exit(42);
  `;

  const instance = new NodeInstance(['--inspect-brk=0'], script);
  const session = await instance.connectInspectorSession();

  // Enable Runtime domain and wait for an event to confirm the session is live.
  await session.send({ method: 'Runtime.enable' });
  await session.waitForNotification('Runtime.executionContextCreated');

  // Resume execution so the script calls inspector.close().
  await session.send({ method: 'Runtime.runIfWaitingForDebugger' });

  // The server should forcibly disconnect us.
  await session.waitForServerDisconnect();

  const { exitCode } = await instance.expectShutdown();
  assert.strictEqual(exitCode, 42);
}

test().then(common.mustCall());
