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

const crypto = require('crypto');

// See #37990 for details on why this is problematic with FIPS.
if (process.config.variables.openssl_is_fips)
  common.skip('Skipping as test uses non-fips compliant EC curve');

// This test will fail for OpenSSL < 1.1.1h
const minOpenSSL = 269488271;

if (crypto.constants.OPENSSL_VERSION_NUMBER < minOpenSSL)
  common.skip('OpenSSL < 1.1.1h');

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

httpsServer.on('listening', () => {
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
});
