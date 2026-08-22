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
// Use a fixed port and run this test sequentially. The assertion below relies
// on nothing else listening on the IPv4 side of the chosen port; with an
// ephemeral port under parallel execution another test can occupy that IPv4
// port, making the connection succeed instead of being refused.
server.listen({
  host,
  port: common.PORT,
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
