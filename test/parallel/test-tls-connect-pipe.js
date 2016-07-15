'use strict';
var common = require('../common');

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

common.refreshTmpDir();

var server = tls.Server(options, common.mustCall(function(socket) {
  server.close();
}));
server.listen(common.PIPE, common.mustCall(function() {
  var options = { rejectUnauthorized: false };
  var client = tls.connect(common.PIPE, options, common.mustCall(function() {
    client.end();
  }));
}));
