'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var util = require('util');
var join = require('path').join;

var options = {
  key: fs.readFileSync(join(common.fixturesDir, 'agent.key')),
  cert: fs.readFileSync(join(common.fixturesDir, 'multi-alice.crt'))
};

var server = tls.createServer(options, function(cleartext) {
  cleartext.end('World');
});
server.listen(0, common.mustCall(function() {
  var socket = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    var peerCert = socket.getPeerCertificate();
    console.error(util.inspect(peerCert));
    assert.deepStrictEqual(
      peerCert.subject.OU,
      ['Information Technology', 'Engineering', 'Marketing']
    );
    server.close();
  }));
  socket.end('Hello');
}));
