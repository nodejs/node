'use strict';

// This tests that net.connect() from a used local port throws EADDRINUSE.

const common = require('../common');
const assert = require('assert');
const net = require('net');

const server1 = net.createServer(common.mustNotCall());
server1.listen(0, common.localhostIPv4, common.mustCall(() => {
  const server2 = net.createServer(common.mustNotCall());
  server2.listen(0, common.localhostIPv4, common.mustCall(() => {
    const client = net.connect({
      host: common.localhostIPv4,
      port: server1.address().port,
      localAddress: common.localhostIPv4,
      localPort: server2.address().port
    }, common.mustNotCall());

    client.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'EADDRINUSE');
      server1.close();
      server2.close();
    }));
  }));
}));
