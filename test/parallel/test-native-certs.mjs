// Flags: --use-system-ca

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
// On macOS:
//   1. To add the certificate:
//    $ security add-trusted-cert \
//       -k /Users/$USER/Library/Keychains/login.keychain-db \
//       test/fixtures/keys/fake-startcom-root-cert.pem
//   2. To remove the certificate:
//     $ security delete-certificate -c 'StartCom Certification Authority' \
//         -t /Users/$USER/Library/Keychains/login.keychain-db
//
// On Windows:
//   1. To add the certificate in PowerShell (remember the thumbprint printed):
//    $ Import-Certificate -FilePath .\test\fixtures\keys\fake-startcom-root-cert.cer \
//       -CertStoreLocation Cert:\CurrentUser\Root
//   2. To remove the certificate by the thumbprint:
//    $  $thumbprint = (Get-ChildItem -Path Cert:\CurrentUser\Root | \
//          Where-Object { $_.Subject -match "StartCom Certification Authority" }).Thumbprint
//    $  Remove-Item -Path "Cert:\CurrentUser\Root\$thumbprint"
//
// On Debian/Ubuntu:
//   1. To add the certificate:
//     $ sudo cp test/fixtures/keys/fake-startcom-root-cert.pem \
//       /usr/local/share/ca-certificates/fake-startcom-root-cert.crt
//     $ sudo update-ca-certificates
//   2. To remove the certificate
//     $ sudo rm /usr/local/share/ca-certificates/fake-startcom-root-cert.crt
//     $ sudo update-ca-certificates --fresh
//
// For other Unix-like systems, consult their manuals, there are usually
// file-based processes similar to the Debian/Ubuntu one but with different
// file locations and update commands.
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
  let server;

  beforeEach(async function() {
    server = https.createServer({
      key: fixtures.readKey('agent8-key.pem'),
      cert: fixtures.readKey('agent8-cert.pem'),
    }, handleRequest);
    server.listen(0);
    await once(server, 'listening');
  });

  it('can connect successfully with a trusted certificate', async function() {
    await fetch(`https://localhost:${server.address().port}/hello-world`);
  });

  afterEach(async function() {
    server?.close();
  });
});
