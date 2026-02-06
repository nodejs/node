'use strict';

// Test that the `'timeout'` event is emitted exactly once if the `timeout`
// option and `request.setTimeout()` are used together.

const { expectsError, mustCall } = require('../common');
const assert = require('assert');
const { createServer, get } = require('http');

const server = createServer(() => {
  // Never respond.
});

server.listen(0, mustCall(() => {
  const req = get({
    port: server.address().port,
    timeout: 2000,
  });

  req.setTimeout(1000);

  req.on('socket', mustCall((socket) => {
    assert.strictEqual(socket.timeout, 2000);

    socket.on('connect', mustCall(() => {
      assert.strictEqual(socket.timeout, 1000);

      // Reschedule the timer to not wait 1 sec and make the test finish faster.
      socket.setTimeout(10);
      assert.strictEqual(socket.timeout, 10);
    }));
  }));

  req.on('error', expectsError({
    name: 'Error',
    code: 'ECONNRESET',
    message: 'socket hang up'
  }));

  req.on('close', mustCall(() => {
    assert.strictEqual(req.destroyed, true);
    server.close();
  }));

  req.on('timeout', mustCall(() => {
    assert.strictEqual(req.socket.listenerCount('timeout'), 1);
    req.destroy();
  }));
}));
