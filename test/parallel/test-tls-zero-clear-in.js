'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const path = require('path');

const cert = fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'));
const key = fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem'));

const server = tls.createServer({
  cert: cert,
  key: key
}, function(c) {
  // Nop
  setTimeout(function() {
    c.end();
    server.close();
  }, 20);
}).listen(0, common.mustCall(function() {
  const conn = tls.connect({
    cert: cert,
    key: key,
    rejectUnauthorized: false,
    port: this.address().port
  }, function() {
    setTimeout(function() {
      conn.destroy();
    }, 20);
  });

  // SSL_write() call's return value, when called 0 bytes, should not be
  // treated as error.
  conn.end('');

  conn.on('error', common.fail);
}));
