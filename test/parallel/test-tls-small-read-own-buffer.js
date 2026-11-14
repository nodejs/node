'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Small TLS reads must not be views over a larger shared buffer: JS keeping
// one of them would keep the whole buffer, and every other read in it, alive.

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const kBlock = 320;
const kBlocks = 3;

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
}, common.mustCall((socket) => {
  let received = 0;
  socket.on('data', common.mustCallAtLeast((chunk) => {
    assert.strictEqual(chunk.byteOffset, 0);
    assert.strictEqual(chunk.buffer.byteLength, chunk.length);
    received += chunk.length;
    if (received === kBlocks * kBlock) {
      socket.end();
      server.close();
    } else if (received % kBlock === 0) {
      // Ask for the next block only once this one has arrived, so that each
      // block is a separate small read.
      socket.write('n');
    }
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
  }, common.mustCall(() => {
    client.write(Buffer.alloc(kBlock));
  }));
  client.on('data', common.mustCall(() => {
    client.write(Buffer.alloc(kBlock));
  }, kBlocks - 1));
}));
