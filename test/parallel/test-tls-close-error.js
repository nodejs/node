'use strict';

var assert = require('assert');
var common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');
var net = require('net');

var errorCount = 0;
var closeCount = 0;

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
}, function(c) {
}).listen(common.PORT, function() {
  var c = tls.connect(common.PORT, function() {
    assert(false, 'should not be called');
  });

  c.on('error', function(err) {
    errorCount++;
  });

  c.on('close', function(err) {
    if (err)
      closeCount++;

    server.close();
  });
});

process.on('exit', function() {
  assert.equal(errorCount, 1);
  assert.equal(closeCount, 1);
});
