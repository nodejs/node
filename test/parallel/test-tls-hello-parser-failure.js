'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const tls = require('tls');

const net = require('net');
const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/test_key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/test_cert.pem')
};

const bonkers = Buffer.alloc(1024 * 1024, 42);

const server = tls.createServer(options, function(c) {

}).listen(0, common.mustCall(function() {
  const client = net.connect(this.address().port, common.mustCall(function() {
    client.write(bonkers);
  }));

  const writeAgain = setImmediate(function() {
    client.write(bonkers);
  });

  client.once('error', common.mustCall(function(err) {
    clearImmediate(writeAgain);
    client.destroy();
    server.close();
  }));

  client.on('close', common.mustCall(function(hadError) {
    assert.strictEqual(hadError, true, 'Client never errored');
  }));
}));
