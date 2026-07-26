'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

// This test starts an https server and tries
// to connect to it using a self-signed certificate.
// This certificate´s keyUsage does not include the keyCertSign
// bit, which used to crash node. The test ensures node
// will not crash. Key and certificate are from #37889.
// Note: This test assumes that the connection will succeed.

if (!common.hasCrypto)
  common.skip('missing crypto');

const { isBoringSSL } = require('../common/crypto');
if (isBoringSSL)
  common.skip('not supported by BoringSSL');

const https = require('https');
const path = require('path');

const key =
  fixtures.readKey(path.join('selfsigned-no-keycertsign', 'key.pem'));

const cert =
  fixtures.readKey(path.join('selfsigned-no-keycertsign', 'cert.pem'));

const serverOptions = {
  key: key,
  cert: cert
};

// Start the server
const httpsServer = https.createServer(serverOptions, (req, res) => {
  res.writeHead(200);
  res.end('hello world\n');
});
httpsServer.listen(0);

httpsServer.on('listening', common.mustCall(() => {
  // Once the server started listening, built the client config
  // with the server´s used port
  const clientOptions = {
    hostname: '127.0.0.1',
    port: httpsServer.address().port,
    ca: cert
  };
  // Try to connect
  const req = https.request(clientOptions, common.mustCall((res) => {
    httpsServer.close();
  }));

  req.on('error', common.mustNotCall());
  req.end();
}));
