'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

var net = require('net');
var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/test_key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/test_cert.pem')
};

var bonkers = new Buffer(1024 * 1024);
bonkers.fill(42);

var server = tls.createServer(options, function(c) {

}).listen(common.PORT, function() {
  var client = net.connect(common.PORT, function() {
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
