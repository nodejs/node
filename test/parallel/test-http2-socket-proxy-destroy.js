'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Calling .destroy() on the socket proxy must not throw
// (previously threw ERR_HTTP2_NO_SOCKET_MANIPULATION) and must
// destroy the Http2Session and the underlying socket.

const server = h2.createServer();

server.on('stream', common.mustCall(function(stream) {
  stream.respond();
  stream.end('ok');
}, 2));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  // Test destroy() without an error.
  {
    const client = h2.connect(`http://localhost:${port}`);
    const request = client.request();
    request.resume();
    request.on('close', common.mustCall(() => {
      const socket = client.socket;
      socket.destroy();
      assert.strictEqual(client.destroyed, true);
      client.on('close', common.mustCall(() => {
        assert.strictEqual(client.socket, undefined);
        // Destroying again through a stale proxy reference must not throw.
        socket.destroy();
      }));
    }));
    client.on('error', common.mustNotCall());
  }

  // Test destroy() with an error.
  {
    const client = h2.connect(`http://localhost:${port}`);
    const request = client.request();
    request.resume();
    request.on('close', common.mustCall(() => {
      client.socket.destroy(new Error('boom'));
      assert.strictEqual(client.destroyed, true);
    }));
    client.on('error', common.mustCall((err) => {
      assert.strictEqual(err.message, 'boom');
    }));
    client.on('close', common.mustCall(() => {
      server.close();
    }));
  }
}));
