'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

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

server.listen(0, function() {
  const req = https.request({ port: this.address().port });
  req.end();

  req.once('error', common.mustCall(function(err) {
    // OpenSSL 1.0.x and 1.1.x use different error messages for junk inputs.
    assert(/unknown protocol/.test(err.message) ||
           /wrong version number/.test(err.message));
    server.close();
  }));
});
