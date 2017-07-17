'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const fixtures = require('../common/fixtures');

let serverConnected = 0;

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const server = tls.Server(options, common.mustCall(function(socket) {
  if (++serverConnected === 2) {
    server.close(common.mustCall());
    server.on('close', common.mustCall());
  }
}, 2));

server.listen(0, function() {
  const client1options = {
    port: this.address().port,
    rejectUnauthorized: false
  };
  const client1 = tls.connect(client1options, common.mustCall(function() {
    client1.end();
  }));

  const client2options = {
    port: this.address().port,
    rejectUnauthorized: false
  };
  const client2 = tls.connect(client2options);
  client2.on('secureConnect', common.mustCall(function() {
    client2.end();
  }));
});
