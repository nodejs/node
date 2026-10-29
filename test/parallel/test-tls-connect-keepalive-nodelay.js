// Flags: --expose-internals
'use strict';

const common = require('../common');

// This test verifies that tls.connect() forwards keepAlive,
// keepAliveInitialDelay, and noDelay options to the underlying socket.

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const {
  kSetNoDelay,
  kSetKeepAlive,
  kSetKeepAliveInitialDelay,
} = require('internal/net');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

// Test: keepAlive, keepAliveInitialDelay, and noDelay
{
  const server = tls.createServer({ key, cert }, (socket) => {
    socket.end();
  });

  server.listen(0, common.mustCall(() => {
    const socket = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
      keepAlive: true,
      keepAliveInitialDelay: 1000,
      noDelay: true,
    }, common.mustCall(() => {
      assert.strictEqual(socket[kSetKeepAlive], true);
      assert.strictEqual(socket[kSetKeepAliveInitialDelay], 1);
      assert.strictEqual(socket[kSetNoDelay], true);
      socket.destroy();
      server.close();
    }));
  }));
}

// Test: defaults (options not set)
{
  const server = tls.createServer({ key, cert }, (socket) => {
    socket.end();
  });

  server.listen(0, common.mustCall(() => {
    const socket = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false,
    }, common.mustCall(() => {
      assert.strictEqual(socket[kSetKeepAlive], false);
      assert.strictEqual(socket[kSetKeepAliveInitialDelay], 0);
      assert.strictEqual(socket[kSetNoDelay], false);
      socket.destroy();
      server.close();
    }));
  }));
}
