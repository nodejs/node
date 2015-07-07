'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var net = require('net');

var common = require('../common');

var buf = new Buffer(10000);
var received = 0;
var ended = 0;
var maxChunk = 768;

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
}, function(c) {
  // Lower and upper limits
  assert(!c.setMaxSendFragment(511));
  assert(!c.setMaxSendFragment(16385));

  // Correct fragment size
  assert(c.setMaxSendFragment(maxChunk));

  c.end(buf);
}).listen(common.PORT, function() {
  var c = tls.connect(common.PORT, {
    rejectUnauthorized: false
  }, function() {
    c.on('data', function(chunk) {
      assert(chunk.length <= maxChunk);
      received += chunk.length;
    });

    // Ensure that we receive 'end' event anyway
    c.on('end', function() {
      ended++;
      c.destroy();
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.equal(ended, 1);
  assert.equal(received, buf.length);
});
