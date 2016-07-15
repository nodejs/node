'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = tls.Server(options, common.mustCall(function(socket) {
  var s = socket.setTimeout(100);
  assert.ok(s instanceof tls.TLSSocket);

  socket.on('timeout', common.mustCall(function(err) {
    socket.end();
    server.close();
  }));
}));

server.listen(0, function() {
  tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  });
});
