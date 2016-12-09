'use strict';
var common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
}, function(c) {
  // Send close-notify without shutting down TCP socket
  if (c._handle.shutdownSSL() !== 1)
    c._handle.shutdownSSL();
}).listen(0, common.mustCall(function() {
  var c = tls.connect(this.address().port, {
    rejectUnauthorized: false
  }, common.mustCall(function() {
    // Ensure that we receive 'end' event anyway
    c.on('end', common.mustCall(function() {
      c.destroy();
      server.close();
    }));
  }));
}));
