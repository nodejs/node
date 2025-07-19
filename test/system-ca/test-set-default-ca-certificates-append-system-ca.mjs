// Flags: --no-use-system-ca

// This tests that tls.setDefaultCACertificates() can be used to remove
// system CA certificates from the default CA store.
// To run this test, install the certificates as described in README.md

import * as common from '../common/index.mjs';
import assert from 'node:assert/strict';
import fixtures from '../common/fixtures.js';
import { once } from 'events';

if (!common.hasCrypto) {
  common.skip('requires crypto');
}

const { default: https } = await import('node:https');
const { default: tls } = await import('node:tls');

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

// First attempt should fail without system certificates.
await assert.rejects(
  fetch(url),
  (err) => {
    assert.strictEqual(err.cause.code, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
    return true;
  },
);

// Now enable system CA certificates
tls.setDefaultCACertificates(tls.getCACertificates('system'));

// Second attempt should succeed.
const response = await fetch(url);
assert.strictEqual(response.status, 200);
const text = await response.text();
assert.strictEqual(text, 'hello world');

server.close();
