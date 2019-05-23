'use strict';

const common = require('../common');

// This test verifies that `tls.connect()` honors the `allowHalfOpen` option.

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');

{
  const socket = tls.connect({ lookup() {} });
  assert.strictEqual(socket.allowHalfOpen, false);
}

{
  const socket = tls.connect({ allowHalfOpen: false, lookup() {} });
  assert.strictEqual(socket.allowHalfOpen, false);
}


const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
}, common.mustCall((socket) => {
  server.close();

  socket.setEncoding('utf8');
  socket.on('data', common.mustCall((chunk) => {
    if (chunk === 'Hello') {
      socket.end(chunk);
    } else {
      assert.strictEqual(chunk, 'Bye');
      assert(!socket.writable);
    }
  }, 2));
}));

server.listen(0, common.mustCall(() => {
  const socket = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
    allowHalfOpen: true,
  }, common.mustCall(() => {
    socket.on('data', common.mustCall((chunk) => {
      assert.strictEqual(chunk, 'Hello');
    }));
    socket.on('end', common.mustCall(() => {
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
