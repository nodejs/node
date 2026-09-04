import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert/strict';
import { execFile, execFileSync } from 'node:child_process';
import fs from 'node:fs';
import http from 'node:http';
import { once } from 'node:events';
import { promisify } from 'node:util';
import { test } from 'node:test';
import tmpdir from '../common/tmpdir.js';
import cryptoFixtures from '../common/crypto.js';

const { opensslCli } = cryptoFixtures;

if (!common.hasCrypto) {
  common.skip('requires crypto');
}

if (process.platform !== 'darwin') {
  common.skip('macOS-specific test');
}

if (!opensslCli) {
  common.skip('missing openssl-cli');
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

async function run(file, args) {
  return execFileAsync(file, args, { encoding: 'utf8' });
}

async function runToCompletion(file, args) {
  await new Promise((resolve) => execFile(file, args, resolve));
}

async function generateCertificates(port) {
  const intermediateKey = tmpdir.resolve('intermediate-key.pem');
  const intermediateCsr = tmpdir.resolve('intermediate.csr');
  const intermediateCert = tmpdir.resolve('intermediate-cert.pem');
  const intermediateDer = tmpdir.resolve('intermediate-cert.der');
  const intermediateConfig = tmpdir.resolve('intermediate.cnf');
  const leafKey = tmpdir.resolve('leaf-key.pem');
  const leafCsr = tmpdir.resolve('leaf.csr');
  const leafCert = tmpdir.resolve('leaf-cert.pem');
  const leafConfig = tmpdir.resolve('leaf.cnf');
  const rootCert = fixtures.path('keys', 'fake-startcom-root-cert.pem');
  const rootKey = fixtures.path('keys', 'fake-startcom-root-key.pem');

  fs.writeFileSync(intermediateConfig, `
[v3_ca]
basicConstraints = critical,CA:TRUE,pathlen:0
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid,issuer
`);
  fs.writeFileSync(leafConfig, `
[v3_leaf]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth,clientAuth
subjectAltName = DNS:localhost,IP:127.0.0.1
authorityInfoAccess = caIssuers;URI:http://127.0.0.1:${port}/intermediate.der,\\
                      OCSP;URI:http://127.0.0.1:${port}/ocsp
`);

  await run(opensslCli, [
    'req', '-new', '-newkey', 'rsa:2048', '-noenc',
    '-keyout', intermediateKey,
    '-out', intermediateCsr,
    '-subj', '/CN=NodeJS Test AIA Intermediate',
  ]);
  await run(opensslCli, [
    'x509', '-req',
    '-in', intermediateCsr,
    '-CA', rootCert,
    '-CAkey', rootKey,
    '-set_serial', `0x${Date.now().toString(16)}01`,
    '-out', intermediateCert,
    '-days', '1',
    '-extfile', intermediateConfig,
    '-extensions', 'v3_ca',
  ]);
  await run(opensslCli, [
    'x509', '-in', intermediateCert, '-outform', 'DER', '-out', intermediateDer,
  ]);
  await run(opensslCli, [
    'req', '-new', '-newkey', 'rsa:2048', '-noenc',
    '-keyout', leafKey,
    '-out', leafCsr,
    '-subj', '/CN=NodeJS Test AIA Leaf',
  ]);
  await run(opensslCli, [
    'x509', '-req',
    '-in', leafCsr,
    '-CA', intermediateCert,
    '-CAkey', intermediateKey,
    '-set_serial', `0x${Date.now().toString(16)}02`,
    '-out', leafCert,
    '-days', '1',
    '-extfile', leafConfig,
    '-extensions', 'v3_leaf',
  ]);

  return {
    intermediateCert,
    intermediateDer,
    leafCert,
    rootCert,
  };
}

function parseKeychainSearchList(stdout) {
  return stdout.trim().split(/\r?\n/)
    .map((line) => line.trim().replace(/^"|"$/g, ''))
    .filter(Boolean);
}

test('system CA enumeration does not fetch AIA or OCSP', {
  timeout: 30_000,
}, async (t) => {
  tmpdir.refresh();

  const requests = [];
  let intermediate;
  const server = http.createServer((req, res) => {
    requests.push({ method: req.method, url: req.url });
    if (req.url === '/intermediate.der') {
      res.writeHead(200, { 'Content-Type': 'application/pkix-cert' });
      res.end(intermediate);
    } else if (req.url?.startsWith('/ocsp')) {
      res.writeHead(500);
      res.end();
    } else {
      res.writeHead(404);
      res.end();
    }
  });
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  t.after(() => new Promise((resolve) => server.close(resolve)));

  const address = server.address();
  assert.notStrictEqual(address, null);
  assert.notStrictEqual(typeof address, 'string');
  const certificates = await generateCertificates(address.port);
  intermediate = fs.readFileSync(certificates.intermediateDer);

  const keychain = tmpdir.resolve('node-system-ca-test.keychain-db');
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
    'add-certificates', '-k', keychain, certificates.leafCert,
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
    '-c', certificates.leafCert,
    '-r', certificates.rootCert,
    '-p', 'ssl',
    '-n', 'localhost',
  ]);
  const validationFetchedAia = requests.some(
    ({ url }) => url === '/intermediate.der',
  );

  requests.length = 0;
  await runToCompletion('/usr/bin/security', [
    'verify-cert',
    '-c', certificates.leafCert,
    '-c', certificates.intermediateCert,
    '-r', certificates.rootCert,
    '-p', 'ssl',
    '-n', 'localhost',
    '-R', 'ocsp',
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
