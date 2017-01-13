'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');

const buf = Buffer.allocUnsafe(10000);
let received = 0;
const maxChunk = 768;

const server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
}, function(c) {
  // Lower and upper limits
  assert(!c.setMaxSendFragment(511));
  assert(!c.setMaxSendFragment(16385));

  // Correct fragment size
  assert(c.setMaxSendFragment(maxChunk));

  c.end(buf);
}).listen(0, common.mustCall(function() {
  const c = tls.connect(this.address().port, {
    rejectUnauthorized: false
  }, common.mustCall(function() {
    c.on('data', function(chunk) {
      assert(chunk.length <= maxChunk);
      received += chunk.length;
    });

    // Ensure that we receive 'end' event anyway
    c.on('end', common.mustCall(function() {
      c.destroy();
      server.close();
      assert.strictEqual(received, buf.length);
    }));
  }));
}));
