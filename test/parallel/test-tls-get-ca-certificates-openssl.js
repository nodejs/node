'use strict';

// This tests that tls.getCACertificates('openssl', cert) looks up
// certificates from OpenSSL's default certificate file and hashed directories
// with certificate inputs.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const { opensslCli } = require('../common/crypto');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

if (!opensslCli) common.skip('missing openssl command');

tmpdir.refresh();

const firstCertDir = tmpdir.resolve('openssl-certs-1');
const secondCertDir = tmpdir.resolve('openssl-certs-2');
fs.mkdirSync(firstCertDir);
fs.mkdirSync(secondCertDir);

function getSubjectHash(certFile) {
  const child = spawnSync(
    opensslCli,
    ['x509', '-hash', '-noout', '-in', certFile],
    { encoding: 'utf8' },
  );
  assert.strictEqual(child.status, 0, child.stderr);
  return child.stdout.trim().split(/\r?\n/)[0];
}

function copyHashDirCertificate(certFile, certDir) {
  const subjectHash = getSubjectHash(certFile);
  fs.copyFileSync(certFile, path.join(certDir, `${subjectHash}.0`));
}

const expectedCertFiles = [
  fixtures.path('keys', 'ca1-cert.pem'),
  fixtures.path('keys', 'ca2-cert.pem'),
  fixtures.path('keys', 'ca3-cert.pem'),
];
const lookupCertFiles = [
  fixtures.path('keys', 'ca1-cert.pem'),
  fixtures.path('keys', 'ca2-cert.pem'),
  fixtures.path('keys', 'agent6-cert.pem'),
];
const unexpectedCertFile = fixtures.path('keys', 'ca4-cert.pem');
const unhashIssuerCertFile = fixtures.path('keys', 'agent10-cert.pem');
const extraCertFile = tmpdir.resolve('extra-certs.pem');
copyHashDirCertificate(expectedCertFiles[1], firstCertDir);
copyHashDirCertificate(expectedCertFiles[2], secondCertDir);
fs.copyFileSync(unexpectedCertFile, path.join(secondCertDir, 'ca4-cert.pem'));
fs.writeFileSync(extraCertFile, [
  fs.readFileSync(unexpectedCertFile, 'utf8'),
  fs.readFileSync(expectedCertFiles[1], 'utf8'),
].join('\n'));

spawnSyncAndExitWithoutError(
  process.execPath,
  ['--use-openssl-ca', fixtures.path('tls-check-openssl-ca-certificates.js')],
  {
    env: {
      ...process.env,
      EXPECTED_CERT_FILES: JSON.stringify(expectedCertFiles),
      LOOKUP_CERT_FILES: JSON.stringify(lookupCertFiles),
      UNEXPECTED_CERT_FILE: unexpectedCertFile,
      UNHASHED_ISSUER_CERT_FILE: unhashIssuerCertFile,
      NODE_EXTRA_CA_CERTS: extraCertFile,
      // Nix-patched OpenSSL gives this precedence over SSL_CERT_FILE.
      NIX_SSL_CERT_FILE: undefined,
      SSL_CERT_FILE: expectedCertFiles[0],
      SSL_CERT_DIR: [firstCertDir, secondCertDir].join(path.delimiter),
    },
  },
);
