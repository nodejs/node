'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const tls = require('tls');
const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

const server = tls.createServer(options, function(s) {
  s.end('hello');
}).listen(0, function() {
  const opts = {
    port: this.address().port,
    rejectUnauthorized: false
  };

  server.on('connection', common.mustCall(function(socket) {
    assert(socket.server === server);
    server.close();
  }));

  const client = tls.connect(opts, function() {
    client.end();
  });
});
