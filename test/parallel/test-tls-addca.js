'use strict';
const common = require('../common');
const fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + '.pem');
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

const caCert = loadPEM('ca1-cert');
const contextWithoutCert = tls.createSecureContext({});
const contextWithCert = tls.createSecureContext({});
// Adding a CA certificate to contextWithCert should not also add it to
// contextWithoutCert. This is tested by trying to connect to a server that
// depends on that CA using contextWithoutCert.
contextWithCert.context.addCACert(caCert);

const serverOptions = {
  key: loadPEM('agent1-key'),
  cert: loadPEM('agent1-cert'),
};
const server = tls.createServer(serverOptions, function() {});

const clientOptions = {
  port: undefined,
  ca: [caCert],
  servername: 'agent1',
  rejectUnauthorized: true,
};

function startTest() {
  // This client should fail to connect because it doesn't trust the CA
  // certificate.
  clientOptions.secureContext = contextWithoutCert;
  clientOptions.port = server.address().port;
  const client = tls.connect(clientOptions, common.fail);
  client.on('error', common.mustCall(() => {
    client.destroy();

    // This time it should connect because contextWithCert includes the needed
    // CA certificate.
    clientOptions.secureContext = contextWithCert;
    const client2 = tls.connect(clientOptions, common.mustCall(() => {
      client2.destroy();
      server.close();
    }));
    client2.on('error', (e) => {
      console.log(e);
    });
  }));
}

server.listen(0, startTest);
