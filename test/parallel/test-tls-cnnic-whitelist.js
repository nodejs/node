'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}

var tls = require('tls');
var fs = require('fs');
var path = require('path');

var error = false;

// agent7-cert.pem is issued by the fake CNNIC root CA so that its
// hash is not listed in the whitelist.
var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'keys/agent7-key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'keys/agent7-cert.pem'))
};

var server = tls.createServer(options, function(s) {
  s.resume();
}).listen(common.PORT, function() {
  var client = tls.connect({
    port: common.PORT,
    rejectUnauthorized: true,
    // fake-cnnic-root-cert has the same subject name as the original
    // rootCA.
    ca: [fs.readFileSync(path.join(common.fixturesDir,
                                   'keys/fake-cnnic-root-cert.pem'))]
  });
  client.on('error', function(e) {
    assert.strictEqual(e.code, 'CERT_REVOKED');
    error = true;
    server.close();
  });
});
process.on('exit', function() {
  assert(error);
});
