// Flags: --expose-internals --no-warnings
/* eslint-disable no-proto */
'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { translatePeerCertificate } = require('internal/tls/common');

const certString = '__proto__=42\nA=1\nB=2\nC=3';

assert.strictEqual(translatePeerCertificate(null), null);
assert.strictEqual(translatePeerCertificate(undefined), null);

assert.strictEqual(translatePeerCertificate(0), null);
assert.strictEqual(translatePeerCertificate(1), 1);

assert.deepStrictEqual(translatePeerCertificate({}), {});

// Earlier versions of Node.js parsed the issuer property but did so
// incorrectly. This behavior has now reached end-of-life and user-supplied
// strings will not be parsed at all.
assert.deepStrictEqual(translatePeerCertificate({ issuer: '' }),
                       { issuer: '' });
assert.deepStrictEqual(translatePeerCertificate({ issuer: null }),
                       { issuer: null });
assert.deepStrictEqual(translatePeerCertificate({ issuer: certString }),
                       { issuer: certString });

// Earlier versions of Node.js parsed the issuer property but did so
// incorrectly. This behavior has now reached end-of-life and user-supplied
// strings will not be parsed at all.
assert.deepStrictEqual(translatePeerCertificate({ subject: '' }),
                       { subject: '' });
assert.deepStrictEqual(translatePeerCertificate({ subject: null }),
                       { subject: null });
assert.deepStrictEqual(translatePeerCertificate({ subject: certString }),
                       { subject: certString });

assert.deepStrictEqual(translatePeerCertificate({ issuerCertificate: '' }),
                       { issuerCertificate: null });
assert.deepStrictEqual(translatePeerCertificate({ issuerCertificate: null }),
                       { issuerCertificate: null });
assert.deepStrictEqual(
  translatePeerCertificate({ issuerCertificate: { subject: certString } }),
  { issuerCertificate: { subject: certString } });

{
  const cert = {};
  cert.issuerCertificate = cert;
  assert.deepStrictEqual(translatePeerCertificate(cert), { issuerCertificate: cert });
}

assert.deepStrictEqual(translatePeerCertificate({ infoAccess: '' }),
                       { infoAccess: { __proto__: null } });
assert.deepStrictEqual(translatePeerCertificate({ infoAccess: null }),
                       { infoAccess: null });
{
  const input =
      '__proto__:mostly harmless\n' +
      'hasOwnProperty:not a function\n' +
      'OCSP - URI:file:///etc/passwd\n';
  const expected = { __proto__: null };
  expected.__proto__ = ['mostly harmless'];
  expected.hasOwnProperty = ['not a function'];
  expected['OCSP - URI'] = ['file:///etc/passwd'];
  assert.deepStrictEqual(translatePeerCertificate({ infoAccess: input }),
                         { infoAccess: expected });
}
