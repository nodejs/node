'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const assert = require('assert');

const cert = fixtures.readSync('test_cert.pem');
const key = fixtures.readSync('test_key.pem');

// https://github.com/nodejs/node/issues/1489
// tls.connect(options) with no options.host should accept a cert with
//   CN:'localhost'
const server = tls.createServer({
  key,
  cert
}).listen(0, common.mustCall(function() {
  const socket = tls.connect({
    port: this.address().port,
    ca: cert,
    // No host set here. 'localhost' is the default,
    // but tls.checkServerIdentity() breaks before the fix with:
    // Error: Hostname/IP doesn't match certificate's altnames:
    //   "Host: undefined. is not cert's CN: localhost"
  }, common.mustCall(function() {
    assert(socket.authorized);
    socket.destroy();
    server.close();
  }));
}));
