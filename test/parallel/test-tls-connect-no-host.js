'use strict';
var common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var assert = require('assert');
var fs = require('fs');
var path = require('path');

var cert = fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'));
var key = fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem'));

// https://github.com/nodejs/node/issues/1489
// tls.connect(options) with no options.host should accept a cert with
//   CN:'localhost'
tls.createServer({
  key: key,
  cert: cert
}).listen(0, function() {
  var socket = tls.connect({
    port: this.address().port,
    ca: cert,
    // No host set here. 'localhost' is the default,
    // but tls.checkServerIdentity() breaks before the fix with:
    // Error: Hostname/IP doesn't match certificate's altnames:
    //   "Host: undefined. is not cert's CN: localhost"
  }, function() {
    assert(socket.authorized);
    process.exit();
  });
});
