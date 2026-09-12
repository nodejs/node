'use strict';
// Tests that when the IPC channel is closed by the other side while messages
// are still queued behind a handle waiting for its acknowledgement, the queue
// is dropped (callbacks are called with ERR_IPC_CHANNEL_CLOSED, or 'error' is
// emitted for messages without a callback) and 'close' is still emitted.
const common = require('../common');
const assert = require('assert');
const net = require('net');
const { fork } = require('child_process');
const fixtures = require('../common/fixtures');

const server = net.createServer().listen(0, common.mustCall(() => {
  // The child exits without ever reading from the channel, so the handle is
  // never acknowledged and everything sent after it stays queued.
  const child = fork(fixtures.path('exit.js'), ['7']);

  let gotExit = false;
  let gotClose = false;

  // The handle itself is written right away.
  child.send('handle', server, common.mustCall((err) => {
    assert.strictEqual(err, null);
  }));

  // Queued behind the handle: with a callback...
  assert.strictEqual(
    child.send('queued', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_IPC_CHANNEL_CLOSED');
      assert.strictEqual(gotClose, false);
    })),
    true,
  );
  // ... without a callback ...
  assert.strictEqual(child.send('queued'), false);
  // ... and with errors swallowed.
  assert.strictEqual(child.send('queued', undefined, { swallowErrors: true }), false);

  child.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_IPC_CHANNEL_CLOSED');
    assert.strictEqual(gotClose, false);
  }));

  child.on('disconnect', common.mustCall(() => {
    assert.strictEqual(child._handleQueue, null);
    assert.strictEqual(child._pendingMessage, null);
  }));

  child.on('exit', common.mustCall((code, signal) => {
    gotExit = true;
    assert.strictEqual(code, 7);
    assert.strictEqual(signal, null);
  }));

  child.on('close', common.mustCall((code, signal) => {
    gotClose = true;
    assert.strictEqual(gotExit, true);
    assert.strictEqual(code, 7);
    assert.strictEqual(signal, null);
    assert.strictEqual(child.connected, false);
    assert.strictEqual(child.channel, null);
    server.close();
  }));
}));
