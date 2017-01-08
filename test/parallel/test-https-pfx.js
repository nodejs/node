'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const pfx = fs.readFileSync(common.fixturesDir + '/test_cert.pfx');

const options = {
  host: '127.0.0.1',
  port: undefined,
  path: '/',
  pfx: pfx,
  passphrase: 'sample',
  requestCert: true,
  rejectUnauthorized: false
};

const server = https.createServer(options, function(req, res) {
  assert.strictEqual(req.socket.authorized, false); // not a client cert
  assert.strictEqual(req.socket.authorizationError,
                     'DEPTH_ZERO_SELF_SIGNED_CERT');
  res.writeHead(200);
  res.end('OK');
});

server.listen(0, options.host, common.mustCall(function() {
  options.port = this.address().port;

  https.get(options, common.mustCall(function(res) {
    let data = '';

    res.on('data', function(data_) { data += data_; });
    res.on('end', common.mustCall(function() {
      assert.strictEqual(data, 'OK');
      server.close();
    }));
  }));
}));
