'use strict';

const common = require('../common');
if (!common.hasIPv6)
  common.skip('no IPv6 support');
if (common.isWindows)
  common.skip('dgram clustering is currently not supported on windows.');

const assert = require('assert');
const cluster = require('cluster');
const dgram = require('dgram');

// This test ensures that the `ipv6Only` option in `dgram.createSock()`
// works as expected.
if (cluster.isMaster) {
  cluster.fork().on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
} else {
  let waiting = 2;
  function close() {
    if (--waiting === 0)
      cluster.worker.disconnect();
  }

  const socket1 = dgram.createSocket({
    type: 'udp6',
    ipv6Only: true
  });
  const socket2 = dgram.createSocket({
    type: 'udp4',
  });
  socket1.on('error', common.mustNotCall());
  socket2.on('error', common.mustNotCall());

  socket1.bind({
    port: 0,
    address: '::',
  }, common.mustCall(() => {
    const { port } = socket1.address();
    socket2.bind({
      port,
      address: '0.0.0.0',
    }, common.mustCall(() => {
      process.nextTick(() => {
        socket1.close(close);
        socket2.close(close);
      });
    }));
  }));
}
