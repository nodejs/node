'use strict';
// This tests the situation where you try to connect a https client
// to an http server. You should get an error and exit.
var common = require('../common');
var http = require('http');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var server = http.createServer(common.fail);

server.listen(0, common.mustCall(function() {
  var req = https.get({ port: this.address().port }, common.fail);

  req.on('error', common.mustCall(function(e) {
    console.log('Got expected error: ', e.message);
    server.close();
  }));
}));
