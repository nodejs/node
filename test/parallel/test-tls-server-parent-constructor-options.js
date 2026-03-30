'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

// Test that `tls.Server` constructor options are passed to the parent
// constructor.

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
};

{
  const server = tls.createServer(options, common.mustCall((socket) => {
    assert.strictEqual(socket.allowHalfOpen, false);
    assert.strictEqual(socket.isPaused(), false);
  }));

  assert.strictEqual(server.allowHalfOpen, false);
  assert.strictEqual(server.pauseOnConnect, false);

  server.listen(0, common.mustCall(() => {
    const socket = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false
    }, common.mustCall(() => {
      socket.end();
    }));

    socket.on('close', () => {
      server.close();
    });
  }));
}

{
  const server = tls.createServer({
    allowHalfOpen: true,
    pauseOnConnect: true,
    ...options
  }, common.mustCall((socket) => {
    assert.strictEqual(socket.allowHalfOpen, true);
    assert.strictEqual(socket.isPaused(), true);
    socket.on('end', socket.end);
  }));

  assert.strictEqual(server.allowHalfOpen, true);
  assert.strictEqual(server.pauseOnConnect, true);

  server.listen(0, common.mustCall(() => {
    const socket = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false
    }, common.mustCall(() => {
      socket.end();
    }));

    socket.on('close', () => {
      server.close();
    });
  }));
}
