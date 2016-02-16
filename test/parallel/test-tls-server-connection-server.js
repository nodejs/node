'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
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
}).listen(common.PORT, function() {
  const opts = {
    port: common.PORT,
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
