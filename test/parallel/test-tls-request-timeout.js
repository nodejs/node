'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

const server = tls.Server(options, common.mustCall(function(socket) {
  const s = socket.setTimeout(100);
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
