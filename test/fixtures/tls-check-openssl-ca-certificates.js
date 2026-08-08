'use strict';

const assert = require('assert');
const { X509Certificate } = require('crypto');
const fs = require('fs');
const tls = require('tls');
const { includesCert } = require('../common/tls');

const expectedCertFiles = process.env.EXPECTED_CERT_FILES ?
  JSON.parse(process.env.EXPECTED_CERT_FILES) : [process.env.SSL_CERT_FILE];
const lookupCertFiles = process.env.LOOKUP_CERT_FILES ?
  JSON.parse(process.env.LOOKUP_CERT_FILES) : expectedCertFiles;

assert.throws(() => tls.getCACertificates(), {
  code: 'ERR_MISSING_ARGS',
});
assert.throws(() => tls.getCACertificates('default'), {
  code: 'ERR_MISSING_ARGS',
});

const expectedCerts = expectedCertFiles.map((certFile) => {
  const cert = fs.readFileSync(certFile, 'utf8');
  const x509 = new X509Certificate(cert);
  return { cert, x509 };
});

const certsBySubject = new Map(
  expectedCerts.map(({ cert, x509 }) => [x509.subject, cert]),
);
const lookupCerts = lookupCertFiles.map((certFile) => {
  const cert = fs.readFileSync(certFile, 'utf8');
  const x509 = new X509Certificate(cert);
  return { cert, x509 };
});

for (const { cert, x509 } of lookupCerts) {
  const issuerCert = certsBySubject.get(x509.issuer);
  assert(issuerCert);
  for (const certInput of [x509, cert, Buffer.from(cert)]) {
    const opensslCertsByCert =
      tls.getCACertificates('openssl', certInput);
    assert(includesCert(opensslCertsByCert, issuerCert));

    const defaultCertsByCert =
      tls.getCACertificates('default', certInput);
    assert(includesCert(defaultCertsByCert, issuerCert));
  }

  const defaultCertsByCert = tls.getCACertificates('default', x509);
  assert.strictEqual(
    defaultCertsByCert,
    tls.getCACertificates('default', x509));
  assert.deepStrictEqual(
    defaultCertsByCert,
    tls.getCACertificates('default', cert));
}

assert.throws(() => tls.getCACertificates('default'), {
  code: 'ERR_MISSING_ARGS',
});

{
  const { x509 } = expectedCerts[1];
  x509.toString = () => ({});
  assert(includesCert(
    tls.getCACertificates('openssl', x509),
    certsBySubject.get(x509.issuer)));
}

{
  const first = expectedCerts[1];
  const second = expectedCerts[0];
  const mutableCert = Buffer.alloc(
    Math.max(Buffer.byteLength(first.cert), Buffer.byteLength(second.cert)),
    '\n');

  mutableCert.write(first.cert);
  assert(includesCert(
    tls.getCACertificates('openssl', mutableCert),
    certsBySubject.get(first.x509.issuer)));

  mutableCert.fill('\n');
  mutableCert.write(second.cert);
  const lookupAfterMutation = tls.getCACertificates('openssl', mutableCert);
  assert(includesCert(
    lookupAfterMutation,
    certsBySubject.get(second.x509.issuer)));
  assert(!includesCert(
    lookupAfterMutation,
    certsBySubject.get(first.x509.issuer)));
}

if (process.env.UNEXPECTED_CERT_FILE) {
  const unexpected = fs.readFileSync(process.env.UNEXPECTED_CERT_FILE, 'utf8');
  const unexpectedX509 = new X509Certificate(unexpected);
  assert(includesCert(
    tls.getCACertificates('openssl', unexpectedX509),
    certsBySubject.get(unexpectedX509.issuer)));
}

if (process.env.UNHASHED_ISSUER_CERT_FILE) {
  const unhashIssuer =
    fs.readFileSync(process.env.UNHASHED_ISSUER_CERT_FILE, 'utf8');
  const unhashIssuerX509 = new X509Certificate(unhashIssuer);
  assert.deepStrictEqual(
    tls.getCACertificates('openssl', unhashIssuerX509),
    []);

  const defaultCerts =
    tls.getCACertificates('default', unhashIssuerX509);
  assert(includesCert(
    defaultCerts,
    fs.readFileSync(process.env.UNEXPECTED_CERT_FILE, 'utf8')));
  assert(!includesCert(defaultCerts, expectedCerts[1].cert));
}

assert.throws(() => tls.getCACertificates('openssl'), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => tls.getCACertificates('openssl', {}), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(
  () => tls.getCACertificates('openssl', 'BAD=x'),
  { code: 'ERR_INVALID_ARG_VALUE' });
assert.throws(
  () => tls.getCACertificates('openssl', Buffer.alloc(0)),
  { code: 'ERR_INVALID_ARG_VALUE' });

tls.setDefaultCACertificates([]);
assert.deepStrictEqual(tls.getCACertificates('default'), []);
