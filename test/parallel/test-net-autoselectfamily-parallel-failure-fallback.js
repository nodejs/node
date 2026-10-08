// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { createConnection, createServer } = require('node:net');
const { internalBinding } = require('internal/test/binding');
const { TCP } = internalBinding('tcp_wrap');
const { UV_ECONNREFUSED } = internalBinding('uv');

// An attempt that fails explicitly starts the next attempt right away, without
// waiting for autoSelectFamilyAttemptTimeout.
const connect = TCP.prototype.connect;
TCP.prototype.connect = function(req, address, port) {
  if (address === '10.0.0.1') {
    setImmediate(() => req.oncomplete(UV_ECONNREFUSED, this, req, false, false));
    return 0;
  }
  return Reflect.apply(connect, this, [req, address, port]);
};

const server = createServer(common.mustCall((socket) => socket.end()));
server.listen(0, '127.0.0.1', common.mustCall(() => {
  const port = server.address().port;
  const connection = createConnection({
    host: 'example.org',
    port,
    lookup: common.mustCall((host, options, callback) => {
      process.nextTick(callback, null, [
        { address: '10.0.0.1', family: 4 },
        { address: '127.0.0.1', family: 4 },
      ]);
    }),
    autoSelectFamily: true,
    autoSelectFamilyAttemptTimeout: common.platformTimeout(60000),
  });

  connection.on('connectionAttemptTimeout', common.mustNotCall());
  connection.on('connectionAttemptFailed', common.mustCall((address, p, family, error) => {
    assert.strictEqual(address, '10.0.0.1');
    assert.strictEqual(error.code, 'ECONNREFUSED');
    process.nextTick(common.mustCall(() => {
      assert.deepStrictEqual(connection.autoSelectFamilyAttemptedAddresses,
                             [`10.0.0.1:${port}`, `127.0.0.1:${port}`]);
    }));
  }));
  connection.on('connect', common.mustCall(() => {
    assert.strictEqual(connection.remoteAddress, '127.0.0.1');
  }));
  connection.on('error', common.mustNotCall());
  connection.on('close', common.mustCall(() => server.close()));
}));
