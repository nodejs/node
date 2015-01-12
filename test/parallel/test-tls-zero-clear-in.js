if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var tls = require('tls');
var path = require('path');

var cert = fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'));
var key = fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem'));

var errorEmitted = false;

var server = tls.createServer({
  cert: cert,
  key: key
}, function(c) {
  // Nop
  setTimeout(function() {
    c.destroy();
    server.close();
  }, 20);
}).listen(common.PORT, function() {
  var conn = tls.connect({
    cert: cert,
    key: key,
    rejectUnauthorized: false,
    port: common.PORT
  }, function() {
    setTimeout(function() {
      conn.destroy();
    }, 20);
  });

  // SSL_write() call's return value, when called 0 bytes, should not be
  // treated as error.
  conn.end('');

  conn.on('error', function(err) {
    console.log(err);
    errorEmitted = true;
  });
});

process.on('exit', function() {
  assert.ok(!errorEmitted);
});
