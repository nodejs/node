'use strict';
// Create an ssl server.  First connection, validate that not resume.
// Cache session and close connection.  Use session on second connection.
// ASSERT resumption.

const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem')
};

const big = Buffer.alloc(2 * 1024 * 1024, 'Y');

// create server
const server = tls.createServer(options, common.mustCall(function(socket) {
  socket.end(big);
  socket.destroySoon();
}));

// start listening
server.listen(0, common.mustCall(function() {
  const client = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    let bytesRead = 0;

    client.on('readable', function() {
      const d = client.read();
      if (d)
        bytesRead += d.length;
    });

    client.on('end', common.mustCall(function() {
      server.close();
      assert.strictEqual(big.length, bytesRead);
    }));
  }));
}));
