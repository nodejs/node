'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that tlsSocket.getFinished() and
// tlsSocket.getPeerFinished() return undefined before
// secure connection is established, and return non-empty
// Buffer objects with Finished messages afterwards, also
// verifying alice.getFinished() == bob.getPeerFinished()
// and alice.getPeerFinished() == bob.getFinished().

const assert = require('assert');
const tls = require('tls');

const msg = {};
const pem = (n) => fixtures.readKey(`${n}.pem`);
const server = tls.createServer({
  key: pem('agent1-key'),
  cert: pem('agent1-cert')
}, common.mustCall((alice) => {
  msg.server = {
    alice: alice.getFinished(),
    bob: alice.getPeerFinished()
  };
  server.close();
}));

server.listen(0, common.mustCall(() => {
  const bob = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false
  }, common.mustCall(() => {
    msg.client = {
      alice: bob.getPeerFinished(),
      bob: bob.getFinished()
    };
    bob.end();
  }));

  msg.before = {
    alice: bob.getPeerFinished(),
    bob: bob.getFinished()
  };
}));

process.on('exit', () => {
  assert.strictEqual(undefined, msg.before.alice);
  assert.strictEqual(undefined, msg.before.bob);

  assert(Buffer.isBuffer(msg.server.alice));
  assert(Buffer.isBuffer(msg.server.bob));
  assert(Buffer.isBuffer(msg.client.alice));
  assert(Buffer.isBuffer(msg.client.bob));

  assert(msg.server.alice.length > 0);
  assert(msg.server.bob.length > 0);
  assert(msg.client.alice.length > 0);
  assert(msg.client.bob.length > 0);

  assert(msg.server.alice.equals(msg.client.alice));
  assert(msg.server.bob.equals(msg.client.bob));
});
