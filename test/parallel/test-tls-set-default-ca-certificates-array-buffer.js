// Flags: --no-use-system-ca
'use strict';

// This tests tls.setDefaultCACertificates() support ArrayBufferView.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const tls = require('tls');
const fixtures = require('../common/fixtures');
const { assertEqualCerts } = require('../common/tls');

const fixtureCert = fixtures.readKey('fake-startcom-root-cert.pem');

// Should accept Buffer.
tls.setDefaultCACertificates([Buffer.from(fixtureCert)]);
const result = tls.getCACertificates('default');
assertEqualCerts(result, [fixtureCert]);

// Reset it to empty.
tls.setDefaultCACertificates([]);
assertEqualCerts(tls.getCACertificates('default'), []);

// Should accept Uint8Array.
const encoder = new TextEncoder();
const uint8Cert = encoder.encode(fixtureCert);
tls.setDefaultCACertificates([uint8Cert]);
const uint8Result = tls.getCACertificates('default');
assertEqualCerts(uint8Result, [fixtureCert]);

// Reset it to empty.
tls.setDefaultCACertificates([]);
assertEqualCerts(tls.getCACertificates('default'), []);

// Should accept DataView.
const dataViewCert = new DataView(uint8Cert.buffer, uint8Cert.byteOffset, uint8Cert.byteLength);
tls.setDefaultCACertificates([dataViewCert]);
const dataViewResult = tls.getCACertificates('default');
assertEqualCerts(dataViewResult, [fixtureCert]);
