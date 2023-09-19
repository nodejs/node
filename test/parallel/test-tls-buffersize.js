'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');

const iter = 10;

const server = tls.createServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem')
}, common.mustCall((socket) => {
  let str = '';
  socket.setEncoding('utf-8');
  socket.on('data', (chunk) => { str += chunk; });

  socket.on('end', common.mustCall(() => {
    assert.strictEqual(str, 'a'.repeat(iter - 1));
    server.close();
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false
  }, common.mustCall(() => {
    assert.strictEqual(client.bufferSize, 0);

    for (let i = 1; i < iter; i++) {
      client.write('a');
      assert.strictEqual(client.bufferSize, i);
    }

    client.on('finish', common.mustCall(() => {
      assert.strictEqual(client.bufferSize, 0);
    }));

    client.end();
  }));
}));
