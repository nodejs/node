'use strict';

// This tests mixed input types for tls.setDefaultCACertificates().

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const tls = require('tls');
const { assertEqualCerts } = require('../common/tls');

const bundledCerts = tls.getCACertificates('bundled');
if (bundledCerts.length < 4) {
  common.skip('Not enough bundled CA certificates available');
}

const encoder = new TextEncoder();

// Test mixed array with string and Buffer.
{
  tls.setDefaultCACertificates([bundledCerts[0], Buffer.from(bundledCerts[1], 'utf8')]);
  const result = tls.getCACertificates('default');
  assertEqualCerts(result, [bundledCerts[0], bundledCerts[1]]);
}

// Test mixed array with string and Uint8Array.
{
  tls.setDefaultCACertificates([bundledCerts[1], encoder.encode(bundledCerts[2])]);
  const result = tls.getCACertificates('default');
  assertEqualCerts(result, [bundledCerts[1], bundledCerts[2]]);
}

// Test mixed array with string and DataView.
{
  const uint8Cert = encoder.encode(bundledCerts[3]);
  const dataViewCert = new DataView(uint8Cert.buffer, uint8Cert.byteOffset, uint8Cert.byteLength);
  tls.setDefaultCACertificates([bundledCerts[1], dataViewCert]);
  const result = tls.getCACertificates('default');
  assertEqualCerts(result, [bundledCerts[1], bundledCerts[3]]);
}

// Test mixed array with Buffer and Uint8Array.
{
  tls.setDefaultCACertificates([Buffer.from(bundledCerts[0], 'utf8'), encoder.encode(bundledCerts[2])]);
  const result = tls.getCACertificates('default');
  assertEqualCerts(result, [bundledCerts[0], bundledCerts[2]]);
}
