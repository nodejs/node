// Env: NODE_USE_SYSTEM_CA=1
// Same as test-native-root-certs.mjs, just testing the environment variable instead of the flag.

import * as common from '../common/index.mjs';
import assert from 'node:assert/strict';
import https from 'node:https';
import fixtures from '../common/fixtures.js';
import { it, beforeEach, afterEach, describe } from 'node:test';
import { once } from 'events';

if (!common.hasCrypto) {
  common.skip('requires crypto');
}

// To run this test, the system needs to be configured to trust
// the CA certificate first (which needs an interactive GUI approval, e.g. TouchID):
// see the README.md in this folder for instructions on how to do this.
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

describe('use-system-ca', function() {

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

  it('trusts a valid root certificate', async function() {
    await fetch(`https://localhost:${server.address().port}/hello-world`);
  });

  afterEach(async function() {
    server?.close();
  });
});
