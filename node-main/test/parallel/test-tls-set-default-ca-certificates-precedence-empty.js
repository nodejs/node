'use strict';

// This tests that per-connection ca option overrides empty default CA certificates

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  res.writeHead(200);
  res.end('per-connection ca works');
}, 1));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const fakeStartcomCert = fixtures.readKey('fake-startcom-root-cert.pem');

  // Set default CA to empty array - connections should normally fail
  tls.setDefaultCACertificates([]);

  // Verify that default CA is empty
  const defaultCerts = tls.getCACertificates('default');
  assert.deepStrictEqual(defaultCerts, []);

  // Connection with per-connection ca option should succeed despite empty default
  const req = https.request({
    hostname: 'localhost',
    port: port,
    path: '/',
    method: 'GET',
    ca: [fakeStartcomCert] // This should override the empty default
  }, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);
    let data = '';
    res.on('data', (chunk) => data += chunk);
    res.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'per-connection ca works');
      server.close();
    }));
  }));

  req.on('error', common.mustNotCall('Should not error with per-connection ca option'));
  req.end();
}));
