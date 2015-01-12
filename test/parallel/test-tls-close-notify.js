if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var assert = require('assert');
var fs = require('fs');
var net = require('net');
var tls = require('tls');

var common = require('../common');

var ended = 0;

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
}, function(c) {
  // Send close-notify without shutting down TCP socket
  if (c.ssl.shutdown() !== 1)
    c.ssl.shutdown();
}).listen(common.PORT, function() {
  var c = tls.connect(common.PORT, {
    rejectUnauthorized: false
  }, function() {
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
});
