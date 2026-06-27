'use strict';
const common = require('../common');
const assert = require('node:assert');
const dgram = require('node:dgram');

// Ensure that bind errors (e.g. EADDRINUSE) are not silently swallowed
// when socket.bind() is called with a callback but without a user
// 'error' handler.

const socket1 = dgram.createSocket('udp4');

socket1.bind(0, common.mustCall(() => {
  const { port } = socket1.address();
  const socket2 = dgram.createSocket('udp4');

  process.on('uncaughtException', common.mustCall((err) => {
    assert.strictEqual(err.code, 'EADDRINUSE');
    socket1.close();
    socket2.close();
  }));

  socket2.bind({ port }, common.mustNotCall());
}));
