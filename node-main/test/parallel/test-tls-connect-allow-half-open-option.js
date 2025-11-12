'use strict';

const common = require('../common');

// This test verifies that `tls.connect()` honors the `allowHalfOpen` option.

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');

{
  const socket = tls.connect({ port: 42, lookup() {} });
  assert.strictEqual(socket.allowHalfOpen, false);
}

{
  const socket = tls.connect({ port: 42, allowHalfOpen: false, lookup() {} });
  assert.strictEqual(socket.allowHalfOpen, false);
}

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
}, common.mustCall((socket) => {
  server.close();

  let message = '';

  socket.setEncoding('utf8');
  socket.on('data', (chunk) => {
    message += chunk;

    if (message === 'Hello') {
      socket.end(message);
      message = '';
    }
  });

  socket.on('end', common.mustCall(() => {
    assert.strictEqual(message, 'Bye');
  }));
}));

server.listen(0, common.mustCall(() => {
  const socket = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
    allowHalfOpen: true,
  }, common.mustCall(() => {
    let message = '';

    socket.on('data', (chunk) => {
      message += chunk;
    });

    socket.on('end', common.mustCall(() => {
      assert.strictEqual(message, 'Hello');

      setTimeout(() => {
        assert(socket.writable);
        assert(socket.write('Bye'));
        socket.end();
      }, 50);
    }));

    socket.write('Hello');
  }));

  socket.setEncoding('utf8');
}));
