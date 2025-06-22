// Flags: --no-use-system-ca

// This tests that tls.useSystemCA() dynamically enables
// system certificates, allowing connections that would otherwise fail.
// To run this test, install the certificates as described in README.md

import * as common from '../common/index.mjs';
import assert from 'node:assert/strict';
import https from 'node:https';
import tls from 'node:tls';
import fixtures from '../common/fixtures.js';
import { it, beforeEach, afterEach, describe } from 'node:test';
import { once } from 'events';

if (!common.hasCrypto) {
  common.skip('requires crypto');
}

const handleRequest = (req, res) => {
  const path = req.url;
  switch (path) {
    case '/hello-world':
      res.writeHead(200);
      res.end('hello world\n');
      break;
    default:
      assert(false, `Unexpected path: ${path}`);
  }
};

describe('enable-system-ca-dynamic', function() {
  async function setupServer(key, cert) {
    const theServer = https.createServer({
      key: fixtures.readKey(key),
      cert: fixtures.readKey(cert),
    }, handleRequest);
    theServer.listen(0);
    await once(theServer, 'listening');

    return theServer;
  }

  let server;

  beforeEach(async function() {
    server = await setupServer('agent8-key.pem', 'agent8-cert.pem');
  });

  it('fails before enabling system CA, succeeds after', async function() {
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
    tls.useSystemCA();

    // Second attempt should succeed.
    const response = await fetch(url);
    assert.strictEqual(response.status, 200);
    const text = await response.text();
    assert.strictEqual(text, 'hello world\n');
  });

  afterEach(async function() {
    server?.close();
  });
});
