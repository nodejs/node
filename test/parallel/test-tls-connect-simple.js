'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');

let serverConnected = 0;

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

const server = tls.Server(options, common.mustCall(function(socket) {
  if (++serverConnected === 2) {
    server.close(common.mustCall(function() {}));
    server.on('close', common.mustCall(function() {}));
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
