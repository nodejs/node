'use strict';
// This tests the situation where you try to connect a https client
// to an http server. You should get an error and exit.
const common = require('../common');
const http = require('http');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const server = http.createServer(common.mustNotCall());

server.listen(0, common.mustCall(function() {
  const req = https.get({ port: this.address().port }, common.mustNotCall());

  req.on('error', common.mustCall(function(e) {
    console.log('Got expected error: ', e.message);
    server.close();
  }));
}));
