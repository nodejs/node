'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const { X509Certificate } = require('crypto');

const expectedPems = tls.getCACertificates({ type: 'default', format: 'pem' });

{
  const certs = tls.getCACertificates({ type: 'default', format: 'x509' });
  assert.strictEqual(certs.length, expectedPems.length);

  for (let i = 0; i < certs.length; i++) {
    const cert = certs[i];
    const expected = new X509Certificate(expectedPems[i]);

    assert.ok(cert instanceof X509Certificate);
  
    assert.strictEqual(cert.fingerprint, expected.fingerprint);
    assert.strictEqual(cert.serialNumber, expected.serialNumber);
    assert.strictEqual(cert.subject, expected.subject);
    assert.strictEqual(cert.raw.toString('hex'), expected.raw.toString('hex'));
  }
}

{
  const certs = tls.getCACertificates({ type: 'default', format: 'buffer' });
  assert.strictEqual(certs.length, expectedPems.length);

  for (let i = 0; i < certs.length; i++) {
    const cert = certs[i];
    const expected = new X509Certificate(expectedPems[i]);

    assert.ok(Buffer.isBuffer(cert));
    assert.strictEqual(cert.toString('hex'), expected.raw.toString('hex'));
  }
}

{
  const certs = tls.getCACertificates({ type: 'default' });
  assert.strictEqual(certs.length, expectedPems.length);
  for (const cert of certs) {
    assert.strictEqual(typeof cert, 'string');
    assert.ok(cert.includes('-----BEGIN CERTIFICATE-----'));
  }
}

{
  assert.throws(() => {
    tls.getCACertificates({ type: 'default', format: 'invalid' });
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: /must be one of/
  });
}

{
  const certs = tls.getCACertificates({ format: 'buffer' });
  assert.ok(Array.isArray(certs));
  assert.ok(certs.length > 0);
  for (const cert of certs) {
    assert.ok(Buffer.isBuffer(cert));
  }
}

{
  assert.throws(() => {
    tls.getCACertificates({ type: 'invalid', format: 'buffer' });
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: /The argument 'type' is invalid/
  });
}
