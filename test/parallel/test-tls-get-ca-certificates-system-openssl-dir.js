'use strict';

// This tests that OpenSSL-style system certificate directories are split
// before loading system CA certificates.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');
if (common.isWindows || common.isMacOS) {
  common.skip('OpenSSL system certificate directories are not used');
}

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const fixtures = require('../common/fixtures');
const { includesCert } = require('../common/tls');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const emptyCertFile = tmpdir.resolve('empty-cert-file.pem');
const firstCertDir = tmpdir.resolve('system-certs-1');
const secondCertDir = tmpdir.resolve('system-certs-2');
fs.writeFileSync(emptyCertFile, '');
fs.mkdirSync(firstCertDir);
fs.mkdirSync(secondCertDir);

const expectedCertFiles = [
  fixtures.path('keys', 'ca1-cert.pem'),
  fixtures.path('keys', 'ca2-cert.pem'),
];
const expectedCerts = expectedCertFiles.map((certFile) =>
  fs.readFileSync(certFile, 'utf8'));

fs.copyFileSync(expectedCertFiles[0], path.join(firstCertDir, 'ca1-cert.pem'));
fs.copyFileSync(expectedCertFiles[1], path.join(secondCertDir, 'ca2-cert.pem'));

const env = {
  ...process.env,
  NODE_EXTRA_CA_CERTS: undefined,
  SSL_CERT_FILE: emptyCertFile,
  SSL_CERT_DIR: [firstCertDir, secondCertDir].join(path.delimiter),
};

function getCACertificates(type, execArgv = []) {
  const certsJSON = tmpdir.resolve(`${type}.json`);
  spawnSyncAndExitWithoutError(process.execPath, [
    ...execArgv,
    fixtures.path('tls-get-ca-certificates.js'),
  ], {
    env: {
      ...env,
      CA_TYPE: type,
      CA_OUT: certsJSON,
    },
  });

  return JSON.parse(fs.readFileSync(certsJSON, 'utf8'));
}

const systemCerts = getCACertificates('system');
const defaultCerts = getCACertificates('default', ['--use-system-ca']);
for (const cert of expectedCerts) {
  assert(includesCert(systemCerts, cert));
  assert(includesCert(defaultCerts, cert));
}
