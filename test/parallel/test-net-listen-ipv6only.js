'use strict';

const common = require('../common');
if (!common.hasIPv6)
  common.skip('no IPv6 support');

// This test ensures that dual-stack support is disabled when
// we specify the `ipv6Only` option in `net.Server.listen()`.
const assert = require('assert');
const net = require('net');

const host = '::';
const server = net.createServer();
server.listen({
  host,
  port: 0,
  ipv6Only: true,
}, common.mustCall(() => {
  const { port } = server.address();
  const socket = net.connect({
    host: '0.0.0.0',
    port,
  });

  socket.on('connect', common.mustNotCall());
  socket.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ECONNREFUSED');
    server.close();
  }));
}));
