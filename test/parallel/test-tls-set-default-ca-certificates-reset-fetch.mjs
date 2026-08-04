// Flags: --no-use-system-ca


// This tests appending certificates to existing defaults should work correctly
// with fetch.

import * as common from '../common/index.mjs';
import { once } from 'node:events';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!common.hasCrypto) common.skip('missing crypto');

const { default: https } = await import('node:https');
const { default: tls } = await import('node:tls');

// Test HTTPS connection fails with bundled CA, succeeds after adding custom CA.
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  res.writeHead(200);
  res.end('hello world');
}, 1));
server.listen(0);
await once(server, 'listening');

const fixturesCert = fixtures.readKey('fake-startcom-root-cert.pem');
tls.setDefaultCACertificates([fixturesCert]);
// First, verify connection works with custom CA.
const response1 = await fetch(`https://localhost:${server.address().port}/custom-ca-test`);
assert.strictEqual(response1.status, 200);
const text1 = await response1.text();
assert.strictEqual(text1, 'hello world');

// Now set empty CA store - connection should fail.
tls.setDefaultCACertificates([]);
// Use IP address to skip session cache.
await assert.rejects(
  fetch(`https://127.0.0.1:${server.address().port}/empty-ca-test`),
  (err) => {
    assert.strictEqual(err.cause.code, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
    return true;
  },
);

server.close();
