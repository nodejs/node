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
// Reserve both address families: an IPv4-only reservation may choose a port
// that is already in use by an IPv6-only socket.
reserved.listen(0, '::', common.mustCall(() => {
  const localPort = reserved.address().port;
  const server = createServer(common.mustCall((socket) => socket.end()));
  // Keep the reservation until the destination listens so it cannot take
  // the source port that the client is about to use.
  server.listen(0, '127.0.0.1', common.mustCall(() => {
    const port = server.address().port;
    assert.notStrictEqual(port, localPort);
    reserved.close(common.mustCall(() => {
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
