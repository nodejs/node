'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const sockets = [];

const server = net.createServer(function(c) {
  c.on('close', common.mustCall());

  sockets.push(c);

  if (sockets.length === 2) {
    assert.strictEqual(server.close(), server);
    sockets.forEach((c) => c.resetAndDestroy());
  }
});

server.on('close', common.mustCall());

assert.strictEqual(server, server.listen(0, () => {
  net.createConnection(server.address().port)
    .on('error', common.mustCall(
      common.expectsError({
        code: 'ECONNRESET',
        name: 'Error'
      }))
    );
  net.createConnection(server.address().port)
    .on('error', common.mustCall(
      common.expectsError({
        code: 'ECONNRESET',
        name: 'Error'
      }))
    );
}));
