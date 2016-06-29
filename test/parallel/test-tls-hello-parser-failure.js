'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var net = require('net');
var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/test_key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/test_cert.pem')
};

var bonkers = Buffer.alloc(1024 * 1024, 42);

var server = tls.createServer(options, function(c) {

}).listen(0, function() {
  var client = net.connect(this.address().port, function() {
    client.write(bonkers);
  });

  var once = false;

  var writeAgain = setTimeout(function() {
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
