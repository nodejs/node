'use strict';

const common = require('../common');
if (!common.hasIPv6)
  common.skip('no IPv6 support');

const dgram = require('dgram');

// This test ensures that dual-stack support is disabled when
// we specify the `ipv6Only` option in `dgram.createSocket()`.
const socket = dgram.createSocket({
  type: 'udp6',
  ipv6Only: true,
});

socket.bind({
  port: 0,
  address: '::',
}, common.mustCall(() => {
  const { port } = socket.address();
  const client = dgram.createSocket('udp4');

  // We can still bind to '0.0.0.0'.
  client.bind({
    port,
    address: '0.0.0.0',
  }, common.mustCall(() => {
    client.close();
    socket.close();
  }));

  client.on('error', common.mustNotCall());
}));
