'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { strictEqual, deepStrictEqual } = require('assert');
const { translatePeerCertificate } = require('_tls_common');

const certString = 'A=1\nB=2\nC=3';
const certObject = { A: '1', B: '2', C: '3' };

strictEqual(translatePeerCertificate(null), null);
strictEqual(translatePeerCertificate(undefined), null);

strictEqual(translatePeerCertificate(0), null);
strictEqual(translatePeerCertificate(1), 1);

deepStrictEqual(translatePeerCertificate({}), {});

deepStrictEqual(translatePeerCertificate({ issuer: '' }),
                { issuer: {} });
deepStrictEqual(translatePeerCertificate({ issuer: null }),
                { issuer: null });
deepStrictEqual(translatePeerCertificate({ issuer: certString }),
                { issuer: certObject });

deepStrictEqual(translatePeerCertificate({ subject: '' }),
                { subject: {} });
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
                { infoAccess: {} });
deepStrictEqual(translatePeerCertificate({ infoAccess: null }),
                { infoAccess: null });
deepStrictEqual(
  translatePeerCertificate({ infoAccess: 'OCSP - URI:file:///etc/passwd' }),
  { infoAccess: { 'OCSP - URI': ['file:///etc/passwd'] } });
