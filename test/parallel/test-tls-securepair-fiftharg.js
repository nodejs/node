'use strict';

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const fs = require('fs');
const tls = require('tls');

const sslcontext = tls.createSecureContext({
  cert: fs.readFileSync(common.fixturesDir + '/test_cert.pem'),
  key: fs.readFileSync(common.fixturesDir + '/test_key.pem')
});

var catchedServername;
const pair = tls.createSecurePair(sslcontext, true, false, false, {
  SNICallback: common.mustCall(function(servername, cb) {
    catchedServername = servername;
  })
});

// captured traffic from browser's request to https://www.google.com
const sslHello = fs.readFileSync(common.fixturesDir + '/google_ssl_hello.bin');

pair.encrypted.write(sslHello);

process.on('exit', function() {
  assert.strictEqual('www.google.com', catchedServername);
});
