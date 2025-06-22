// Flags: --no-use-system-ca


// This tests appending certificates to existing defaults should work correctly
// with fetch.

import * as common from '../common/index.mjs';
import { includesCert } from '../common/tls.js';
import { once } from 'node:events';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!common.hasCrypto) common.skip('missing crypto');

const { default: https } = await import('node:https');
const { default: tls } = await import('node:tls');

const bundledCerts = tls.getCACertificates('bundled');
const fixtureCert = fixtures.readKey('fake-startcom-root-cert.pem');
if (includesCert(bundledCerts, fixtureCert)) {
  common.skip('fake-startcom-root-cert is already in bundled certificates, skipping test');
}

// Test HTTPS connection fails with bundled CA, succeeds after adding custom CA
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  res.writeHead(200);
  res.end('hello world');
}, 1));
server.listen(0);
await once(server, 'listening');
const url = `https://localhost:${server.address().port}/hello-world`;

// First attempt should fail without custom CA.
await assert.rejects(
  fetch(url),
  (err) => {
    assert.strictEqual(err.cause.code, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
    return true;
  },
);

// Now enable custom CA certificate.
tls.setDefaultCACertificates([fixtureCert]);

// Second attempt should succeed.
const response = await fetch(url);
assert.strictEqual(response.status, 200);
const text = await response.text();
assert.strictEqual(text, 'hello world');

server.close();
