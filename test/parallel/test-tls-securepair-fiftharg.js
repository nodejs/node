'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const sslcontext = tls.createSecureContext({
  cert: fixtures.readSync('test_cert.pem'),
  key: fixtures.readSync('test_key.pem')
});

let catchedServername;
const pair = tls.createSecurePair(sslcontext, true, false, false, {
  SNICallback: common.mustCall(function(servername, cb) {
    catchedServername = servername;
  })
});

// captured traffic from browser's request to https://www.google.com
const sslHello = fixtures.readSync('google_ssl_hello.bin');

pair.encrypted.write(sslHello);

process.on('exit', function() {
  assert.strictEqual('www.google.com', catchedServername);
});
