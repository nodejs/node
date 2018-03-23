// Tests that a child's internal handle queue and pending handle get cleared
// when the child exits, calling all provided callbacks (or emitting errors).

'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const cp = require('child_process');
const fixtures = require('../common/fixtures');

const fixture = fixtures.path('exit.js');

function testErrorCallOnce() {
  return common.expectsError({
    type: Error,
    message: 'Channel closed',
    code: 'ERR_IPC_CHANNEL_CLOSED'
  }, 1);
}

const server = net.createServer((s) => {
  if (common.isWindows) {
    s.on('error', function(err) {
      // Prevent possible ECONNRESET errors from popping up
      if (err.code !== 'ECONNRESET')
        throw err;
    });
  }
  setTimeout(function() {
    s.destroy();
  }, 100);
}).listen(0, common.mustCall((error) => {
  const child = cp.fork(fixture);

  let gotExit = false;

  function send(callback) {
    const s = net.connect(server.address().port, function() {
      child.send({}, s, callback);
    });

    // https://github.com/nodejs/node/issues/3635#issuecomment-157714683
    // ECONNREFUSED or ECONNRESET errors can happen if this connection is still
    // establishing while the server has already closed.
    // EMFILE can happen if the worker __and__ the server had already closed.
    s.on('error', function(err) {
      if ((err.code !== 'ECONNRESET') &&
          (err.code !== 'ECONNREFUSED') &&
          (err.code !== 'EMFILE')) {
        throw err;
      }
    });
  }

  // Pending handle get written and callback gets
  // called immediately (before 'exit')
  send(common.mustCall((error) => {
    assert.strictEqual(error, null);
    assert.strictEqual(gotExit, false);
  }));

  // Handle gets put into handle queue
  send(testErrorCallOnce());

  // Handle also gets put into handle queue, but without
  // a callback, so the error should be emitted instead
  send();

  child.on('error', testErrorCallOnce());

  child.on('exit', common.mustCall((code, signal) => {
    gotExit = true;
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  }));

  child.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);

    assert.strictEqual(child._handleQueue, null);
    assert.strictEqual(child._pendingMessage, null);

    server.close();
  }));
}));
