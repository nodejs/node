'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const assert = require('assert');
const https = require('https');
const net = require('net');

const server = net.createServer(function(s) {
  s.once('data', function() {
    s.end('I was waiting for you, hello!', function() {
      s.destroy();
    });
  });
});

server.listen(0, common.mustCall(function() {
  const req = https.request({ port: this.address().port });
  req.end();

  // Different OpenSSL versions report different errors for junk data on a
  // TLS connection, depending on which record validation check fires first.
  const expectedErrorMessage =
    /wrong version number|packet length too long|bad record type/;
  req.once('error', common.mustCall(function(err) {
    assert.match(err.message, expectedErrorMessage);
    server.close();
  }));
}));
