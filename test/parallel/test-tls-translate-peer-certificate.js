// Flags: --expose-internals --no-warnings
/* eslint-disable no-proto */
'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { strictEqual, deepStrictEqual } = require('assert');
const { translatePeerCertificate } = require('internal/tls/common');

const certString = '__proto__=42\nA=1\nB=2\nC=3';

strictEqual(translatePeerCertificate(null), null);
strictEqual(translatePeerCertificate(undefined), null);

strictEqual(translatePeerCertificate(0), null);
strictEqual(translatePeerCertificate(1), 1);

deepStrictEqual(translatePeerCertificate({}), {});

// Earlier versions of Node.js parsed the issuer property but did so
// incorrectly. This behavior has now reached end-of-life and user-supplied
// strings will not be parsed at all.
deepStrictEqual(translatePeerCertificate({ issuer: '' }),
                { issuer: '' });
deepStrictEqual(translatePeerCertificate({ issuer: null }),
                { issuer: null });
deepStrictEqual(translatePeerCertificate({ issuer: certString }),
                { issuer: certString });

// Earlier versions of Node.js parsed the issuer property but did so
// incorrectly. This behavior has now reached end-of-life and user-supplied
// strings will not be parsed at all.
deepStrictEqual(translatePeerCertificate({ subject: '' }),
                { subject: '' });
deepStrictEqual(translatePeerCertificate({ subject: null }),
                { subject: null });
deepStrictEqual(translatePeerCertificate({ subject: certString }),
                { subject: certString });

deepStrictEqual(translatePeerCertificate({ issuerCertificate: '' }),
                { issuerCertificate: null });
deepStrictEqual(translatePeerCertificate({ issuerCertificate: null }),
                { issuerCertificate: null });
deepStrictEqual(
  translatePeerCertificate({ issuerCertificate: { subject: certString } }),
  { issuerCertificate: { subject: certString } });

{
  const cert = {};
  cert.issuerCertificate = cert;
  deepStrictEqual(translatePeerCertificate(cert), { issuerCertificate: cert });
}

deepStrictEqual(translatePeerCertificate({ infoAccess: '' }),
                { infoAccess: { __proto__: null } });
deepStrictEqual(translatePeerCertificate({ infoAccess: null }),
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
  deepStrictEqual(translatePeerCertificate({ infoAccess: input }),
                  { infoAccess: expected });
}
