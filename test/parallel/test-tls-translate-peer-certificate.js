/* eslint-disable no-proto */
'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { strictEqual, deepStrictEqual } = require('assert');
const { translatePeerCertificate } = require('_tls_common');

const certString = '__proto__=42\nA=1\nB=2\nC=3';
const certObject = Object.create(null);
certObject.__proto__ = '42';
certObject.A = '1';
certObject.B = '2';
certObject.C = '3';

strictEqual(translatePeerCertificate(null), null);
strictEqual(translatePeerCertificate(undefined), null);

strictEqual(translatePeerCertificate(0), null);
strictEqual(translatePeerCertificate(1), 1);

deepStrictEqual(translatePeerCertificate({}), {});

deepStrictEqual(translatePeerCertificate({ issuer: '' }),
                { issuer: Object.create(null) });
deepStrictEqual(translatePeerCertificate({ issuer: null }),
                { issuer: null });
deepStrictEqual(translatePeerCertificate({ issuer: certString }),
                { issuer: certObject });

deepStrictEqual(translatePeerCertificate({ subject: '' }),
                { subject: Object.create(null) });
deepStrictEqual(translatePeerCertificate({ subject: null }),
                { subject: null });
deepStrictEqual(translatePeerCertificate({ subject: certString }),
                { subject: certObject });

deepStrictEqual(translatePeerCertificate({ issuerCertificate: '' }),
                { issuerCertificate: null });
deepStrictEqual(translatePeerCertificate({ issuerCertificate: null }),
                { issuerCertificate: null });
deepStrictEqual(
  translatePeerCertificate({ issuerCertificate: { subject: certString } }),
  { issuerCertificate: { subject: certObject } });

{
  const cert = {};
  cert.issuerCertificate = cert;
  deepStrictEqual(translatePeerCertificate(cert), { issuerCertificate: cert });
}

deepStrictEqual(translatePeerCertificate({ infoAccess: '' }),
                { infoAccess: Object.create(null) });
deepStrictEqual(translatePeerCertificate({ infoAccess: null }),
                { infoAccess: null });
{
  const input =
      '__proto__:mostly harmless\n' +
      'hasOwnProperty:not a function\n' +
      'OCSP - URI:file:///etc/passwd\n';
  const expected = Object.create(null);
  expected.__proto__ = ['mostly harmless'];
  expected.hasOwnProperty = ['not a function'];
  expected['OCSP - URI'] = ['file:///etc/passwd'];
  deepStrictEqual(translatePeerCertificate({ infoAccess: input }),
                  { infoAccess: expected });
}
