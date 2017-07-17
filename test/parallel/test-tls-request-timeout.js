'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
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
