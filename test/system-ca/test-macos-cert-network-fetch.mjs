import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert/strict';
import { X509Certificate } from 'node:crypto';
import { execFile, execFileSync } from 'node:child_process';
import http from 'node:http';
import { once } from 'node:events';
import { promisify } from 'node:util';
import { test } from 'node:test';

if (!common.hasCrypto) {
  common.skip('requires crypto');
}

if (process.platform !== 'darwin') {
  common.skip('macOS-specific test');
}

function isCertInKeychain(cn) {
  try {
    execFileSync('security', ['find-certificate', '-c', cn], { stdio: 'pipe' });
    return true;
  } catch {
    return false;
  }
}

if (!isCertInKeychain('StartCom Certification Authority')) {
  common.skip(
    'fake-startcom-root-cert.pem not found in system CA store. ' +
    'Please follow setup instructions in test/system-ca/README.md',
  );
}

const execFileAsync = promisify(execFile);
const responderPort = 12347;

async function run(file, args) {
  return execFileAsync(file, args, { encoding: 'utf8' });
}

async function runToCompletion(file, args) {
  await new Promise((resolve) => execFile(file, args, resolve));
}

function parseKeychainSearchList(stdout) {
  return stdout.trim().split(/\r?\n/)
    .map((line) => line.trim().replace(/^"|"$/g, ''))
    .filter(Boolean);
}

test('system CA enumeration does not fetch AIA or OCSP', {
  timeout: 30_000,
}, async (t) => {
  const requests = [];
  const leafCert = fixtures.path('keys', 'system-ca-network-leaf-cert.pem');
  const rootCert = fixtures.path('keys', 'fake-startcom-root-cert.pem');
  const infoAccess = new X509Certificate(
    fixtures.readKey('system-ca-network-leaf-cert.pem'),
  ).infoAccess;
  assert.match(
    infoAccess,
    /CA Issuers - URI:http:\/\/127\.0\.0\.1:12347\/intermediate\.der/,
  );
  assert.match(infoAccess, /OCSP - URI:http:\/\/127\.0\.0\.1:12347\/ocsp/);

  const server = http.createServer((req, res) => {
    requests.push({ method: req.method, url: req.url });
    if (req.url === '/intermediate.der') {
      res.writeHead(404, { 'Cache-Control': 'no-store' });
      res.end();
    } else if (req.url?.startsWith('/ocsp')) {
      res.writeHead(500, { 'Cache-Control': 'no-store' });
      res.end();
    } else {
      res.writeHead(404);
      res.end();
    }
  });
  server.listen(responderPort, '127.0.0.1');
  await once(server, 'listening');
  t.after(() => new Promise((resolve) => server.close(resolve)));

  const keychain = '/tmp/node-system-ca-network-test.keychain-db';
  const password = 'node-test';
  const { stdout } = await run('/usr/bin/security', [
    'list-keychains', '-d', 'user',
  ]);
  const originalKeychains = parseKeychainSearchList(stdout);

  await run('/usr/bin/security', [
    'create-keychain', '-p', password, keychain,
  ]);
  t.after(async () => {
    await run('/usr/bin/security', [
      'list-keychains', '-d', 'user', '-s', ...originalKeychains,
    ]);
    await run('/usr/bin/security', ['delete-keychain', keychain]);
  });
  await run('/usr/bin/security', [
    'unlock-keychain', '-p', password, keychain,
  ]);
  await run('/usr/bin/security', [
    'set-keychain-settings', '-lut', '3600', keychain,
  ]);
  await run('/usr/bin/security', [
    'add-certificates', '-k', keychain, leafCert,
  ]);
  await run('/usr/bin/security', [
    'list-keychains', '-d', 'user', '-s', ...originalKeychains, keychain,
  ]);

  await run(process.execPath, [
    '-e', 'require("node:tls").getCACertificates("system")',
  ]);
  const enumerationRequests = [...requests];
  requests.length = 0;

  // Node's TLS validation uses OpenSSL and does not fetch AIA or OCSP itself.
  // Use macOS trust evaluation as a control to show that the generated
  // certificate can trigger both types of network request.
  await runToCompletion('/usr/bin/security', [
    'verify-cert',
    '-c', leafCert,
    '-r', rootCert,
    '-p', 'basic',
    '-R', 'ocsp',
    '-R', 'online',
    '-R', 'require',
  ]);
  const validationFetchedAia = requests.some(
    ({ url }) => url === '/intermediate.der',
  );

  requests.length = 0;
  await runToCompletion('/usr/bin/security', [
    'verify-cert',
    '-c', leafCert,
    '-c', fixtures.path(
      'keys',
      'system-ca-network-intermediate-cert.pem',
    ),
    '-r', rootCert,
    '-p', 'basic',
    '-R', 'ocsp',
    '-R', 'online',
    '-R', 'require',
  ]);
  const validationRequestedOcsp = requests.some(
    ({ url }) => url?.startsWith('/ocsp'),
  );

  assert.deepStrictEqual({
    enumerationRequests,
    validationFetchedAia,
    validationRequestedOcsp,
  }, {
    enumerationRequests: [],
    validationFetchedAia: true,
    validationRequestedOcsp: true,
  });
});
