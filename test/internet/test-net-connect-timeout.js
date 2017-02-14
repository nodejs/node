'use strict';
// This example attempts to time out before the connection is established
// https://groups.google.com/forum/#!topic/nodejs/UE0ZbfLt6t8
// https://groups.google.com/forum/#!topic/nodejs-dev/jR7-5UDqXkw

const common = require('../common');
const net = require('net');
const assert = require('assert');

const start = new Date();

const T = 100;

// 192.0.2.1 is part of subnet assigned as "TEST-NET" in RFC 5737.
// For use solely in documentation and example source code.
// In short, it should be unreachable.
// In practice, it's a network black hole.
const socket = net.createConnection(9999, '192.0.2.1');

socket.setTimeout(T);

socket.on('timeout', common.mustCall(function() {
  console.error('timeout');
  const now = new Date();
  assert.ok(now - start < T + 500);
  socket.destroy();
}));

socket.on('connect', common.mustNotCall());
