'use strict';

// This tests that per-connection ca option overrides bundled default CA certificates.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { includesCert } = require('../common/tls');

const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  res.writeHead(200);
  res.end('override works');
}, 1));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const bundledCerts = tls.getCACertificates('bundled');
  const fakeStartcomCert = fixtures.readKey('fake-startcom-root-cert.pem');

  // Set default CA to bundled certs (which don't include fake-startcom-root-cert)
  tls.setDefaultCACertificates(bundledCerts);

  // Verify that fake-startcom-root-cert is not in default
  const defaultCerts = tls.getCACertificates('default');
  assert(!includesCert(defaultCerts, fakeStartcomCert));

  // Connection with per-connection ca should succeed despite wrong default
  const req = https.request({
    hostname: 'localhost',
    port: port,
    path: '/',
    method: 'GET',
    ca: [fakeStartcomCert] // This should override the bundled defaults
  }, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);
    let data = '';
    res.on('data', (chunk) => data += chunk);
    res.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'override works');
      server.close();
    }));
  }));

  req.on('error', common.mustNotCall('Should not error with per-connection ca option'));
  req.end();
}));
