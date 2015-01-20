if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var assert = require('assert');
var fs = require('fs');
var net = require('net');
var tls = require('tls');

var common = require('../common');

var received = '';
var ended = 0;

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
}, function(c) {
  c._write('hello ', null, function() {
    c._write('world!', null, function() {
      c.destroy();
    });
    c._write(' gosh', null, function() {});
  });

  server.close();
}).listen(common.PORT, function() {
  var c = tls.connect(common.PORT, {
    rejectUnauthorized: false
  }, function() {
    c.on('data', function(chunk) {
      received += chunk;
    });
    c.on('end', function() {
      ended++;
    });
  });
});

process.on('exit', function() {
  assert.equal(ended, 1);
  assert.equal(received, 'hello world! gosh');
});
