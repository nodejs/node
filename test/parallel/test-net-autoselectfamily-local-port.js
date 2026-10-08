'use strict';

const common = require('../common');
if (!common.hasIPv6) common.skip('IPv6 loopback is unavailable');

const assert = require('node:assert');
const { createConnection, createServer } = require('node:net');
const { TCP } = process.binding('tcp_wrap');
// A pending IPv6 connect with a fixed source port must be closed before an
// IPv4 candidate binds to that port.
TCP.prototype.connect6 = function() {
  return 0;
};

const reserved = createServer();
reserved.listen(0, '127.0.0.1', common.mustCall(() => {
  const localPort = reserved.address().port;
  reserved.close(common.mustCall(() => {
    const server = createServer(common.mustCall((socket) => socket.end()));
    server.listen(0, '127.0.0.1', common.mustCall(() => {
      const port = server.address().port;
      const connection = createConnection({
        host: 'example.org',
        port,
        localPort,
        lookup: common.mustCall((host, options, callback) => {
          process.nextTick(callback, null, [
            { address: '::1', family: 6 },
            { address: '127.0.0.1', family: 4 },
          ]);
        }),
        autoSelectFamily: true,
        autoSelectFamilyAttemptTimeout: 10,
      });
      connection.on('connect', common.mustCall(() => {
        assert.strictEqual(connection.localPort, localPort);
        assert.strictEqual(connection.remoteAddress, '127.0.0.1');
      }));
      connection.on('error', common.mustNotCall());
      connection.on('close', common.mustCall(() => server.close()));
    }));
  }));
}));
