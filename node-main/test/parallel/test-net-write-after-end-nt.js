'use strict';
const common = require('../common');

const assert = require('assert');
const net = require('net');

const { expectsError, mustCall } = common;

// This test ensures those errors caused by calling `net.Socket.write()`
// after sockets ending will be emitted in the next tick.
const server = net.createServer(mustCall((socket) => {
  socket.end();
})).listen(() => {
  const client = net.connect(server.address().port, () => {
    let hasError = false;
    client.on('error', mustCall((err) => {
      hasError = true;
      server.close();
    }));
    client.on('end', mustCall(() => {
      const ret = client.write('hello', expectsError({
        code: 'EPIPE',
        message: 'This socket has been ended by the other party',
        name: 'Error'
      }));

      assert.strictEqual(ret, false);
      assert(!hasError, 'The error should be emitted in the next tick.');
    }));
    client.end();
  });
});
