'use strict';

// This tests appending certificates to existing defaults should work correctly
// with https.request().

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { includesCert } = require('../common/tls');

const bundledCerts = tls.getCACertificates('bundled');
const fixtureCert = fixtures.readKey('fake-startcom-root-cert.pem');
if (includesCert(bundledCerts, fixtureCert)) {
  common.skip('fake-startcom-root-cert is already in bundled certificates, skipping test');
}

// Test HTTPS connection fails with bundled CA, succeeds after adding custom CA
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, (req, res) => {
  res.writeHead(200);
  res.end('success');
});

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  // Set to bundled CA certificates - connection should fail
  tls.setDefaultCACertificates(bundledCerts);

  const req1 = https.request({
    hostname: 'localhost',
    port: port,
    path: '/',
    method: 'GET'
  }, common.mustNotCall('Should not succeed with bundled CA only'));

  req1.on('error', common.mustCall((err) => {
    console.log(err);
    // Should fail with certificate verification error
    assert.strictEqual(err.code, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');

    // Now add the fake-startcom-root-cert to bundled certs - connection should succeed
    tls.setDefaultCACertificates([...bundledCerts, fixtureCert]);

    const req2 = https.request({
      hostname: 'localhost',
      port: port,
      path: '/',
      method: 'GET'
    }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);
      let data = '';
      res.on('data', (chunk) => data += chunk);
      res.on('end', common.mustCall(() => {
        assert.strictEqual(data, 'success');
        server.close();
      }));
    }));

    req2.on('error', common.mustNotCall('Should not error with correct CA added'));
    req2.end();
  }));

  req1.end();
}));
