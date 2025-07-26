// Flags: --use-system-ca


// This tests that tls.setDefaultCACertificates() can be used to dynamically
// enable system CA certificates for HTTPS connections.
// To run this test, install the certificates as described in README.md

import * as common from '../common/index.mjs';
import assert from 'node:assert/strict';
import fixtures from '../common/fixtures.js';
import { once } from 'events';
import { includesCert, assertEqualCerts } from '../common/tls.js';

if (!common.hasCrypto) {
  common.skip('requires crypto');
}

const { default: https } = await import('node:https');
const { default: tls } = await import('node:tls');

// Verify that system CA includes the fake-startcom-root-cert.
const systemCerts = tls.getCACertificates('system');
const fixturesCert = fixtures.readKey('fake-startcom-root-cert.pem');
if (!includesCert(systemCerts, fixturesCert)) {
  common.skip('fake-startcom-root-cert.pem not found in system CA store. ' +
              'Please follow setup instructions in test/system-ca/README.md');
}
const bundledCerts = tls.getCACertificates('bundled');
if (includesCert(bundledCerts, fixturesCert)) {
  common.skip('fake-startcom-root-cert.pem should not be in bundled CA store');
}

const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  const path = req.url;
  switch (path) {
    case '/system-ca-test':
      res.writeHead(200);
      res.end('system ca works');
      break;
    case '/bundled-ca-test':
      res.writeHead(200);
      res.end('bundled ca works');
      break;
    default:
      assert(false, `Unexpected path: ${path}`);
  }
}, 1));


server.listen(0);
await once(server, 'listening');

const url = `https://localhost:${server.address().port}`;

// First, verify connection works with system CA (including fake-startcom-root-cert)
const response1 = await fetch(`${url}/system-ca-test`);
assert.strictEqual(response1.status, 200);
const text1 = await response1.text();
assert.strictEqual(text1, 'system ca works');

// Now override with bundled certs (which do not include fake-startcom-root-cert)
tls.setDefaultCACertificates(bundledCerts);

// Connection should now fail because fake-startcom-root-cert is no longer in the CA store.
// Use IP address to skip session cache.
await assert.rejects(
  fetch(`https://127.0.0.1:${server.address().port}/bundled-ca-test`),
  (err) => {
    assert.strictEqual(err.cause.code, 'SELF_SIGNED_CERT_IN_CHAIN');
    return true;
  },
);

// Verify that system CA type still returns original system certs
const stillSystemCerts = tls.getCACertificates('system');
assertEqualCerts(stillSystemCerts, systemCerts);
assert(includesCert(stillSystemCerts, fixturesCert));

// Verify that default CA now returns bundled certs
const currentDefaults = tls.getCACertificates('default');
assertEqualCerts(currentDefaults, bundledCerts);
assert(!includesCert(currentDefaults, fixturesCert));

server.close();
