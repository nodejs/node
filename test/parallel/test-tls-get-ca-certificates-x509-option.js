'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const { X509Certificate } = require('crypto');

{
  const certs = tls.getCACertificates({ type: 'default', format: 'x509' });

  assert.ok(Array.isArray(certs), 'should return an array');
  assert.ok(certs.length > 0, 'should return non-empty array');

  for (let i = 0; i < certs.length; i++) {
    assert.ok(certs[i] instanceof X509Certificate,
              `cert at index ${i} should be instance of X509Certificate`);
  }

  assert.match(certs[0].serialNumber, /^[0-9A-F]+$/i,
               'serialNumber should be hex string');
}

{
  const certs = tls.getCACertificates({ type: 'default', format: 'buffer' });
  assert.ok(Array.isArray(certs));
  assert.ok(certs.length > 0);

  for (let i = 0; i < certs.length; i++) {
    assert.ok(Buffer.isBuffer(certs[i]),
              `cert at index ${i} should be a Buffer`);
  }
}

{
  const certs = tls.getCACertificates({ type: 'default' });
  assert.ok(Array.isArray(certs));
  assert.ok(certs.length > 0);
  for (let i = 0; i < certs.length; i++) {
    assert.strictEqual(typeof certs[i], 'string');
    assert.ok(certs[i].includes('-----BEGIN CERTIFICATE-----'));
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

  for (let i = 0; i < certs.length; i++) {
    assert.ok(Buffer.isBuffer(certs[i]),
              `cert at index ${i} should be a Buffer`);
  }
}

{
  assert.throws(() => {
    tls.getCACertificates({ type: 'invalid', format: 'buffer' });
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The argument 'type' is invalid. Received 'invalid'"
  });
}

{
  const certs = tls.getCACertificates({ format: 'string', encoding: 'der' });
  assert.ok(Array.isArray(certs));
  assert.ok(certs.length > 0);
  for (let i = 0; i < certs.length; i++) {
    assert.strictEqual(typeof certs[i], 'string');
    assert.ok(/^[A-Za-z0-9+/]+=*$/.test(certs[i]), 'should be base64 string');
  }
}
