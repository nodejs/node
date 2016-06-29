'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const path = require('path');

const keysDir = path.join(common.fixturesDir, 'keys');

const ca = fs.readFileSync(path.join(keysDir, 'ca1-cert.pem'));
const cert = fs.readFileSync(path.join(keysDir, 'agent1-cert.pem'));
const key = fs.readFileSync(path.join(keysDir, 'agent1-key.pem'));

const server = tls.createServer({
  cert: cert,
  key: key
}, function(c) {
  c.end();
}).listen(0, function() {
  const secureContext = tls.createSecureContext({
    ca: ca
  });

  const socket = tls.connect({
    secureContext: secureContext,
    servername: 'agent1',
    port: this.address().port
  }, common.mustCall(function() {
    server.close();
    socket.end();
  }));
});
