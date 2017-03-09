'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const net = require('net');
const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/test_key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/test_cert.pem')
};

const bonkers = new Buffer(1024 * 1024);
bonkers.fill(42);

const server = tls.createServer(options, function(c) {

}).listen(0, function() {
  const client = net.connect(this.address().port, function() {
    client.write(bonkers);
  });

  let once = false;

  const writeAgain = setTimeout(function() {
    client.write(bonkers);
  });

  client.on('error', function(err) {
    if (!once) {
      clearTimeout(writeAgain);
      once = true;
      client.destroy();
      server.close();
    }
  });

  client.on('close', function(hadError) {
    assert.strictEqual(hadError, true, 'Client never errored');
  });
});
