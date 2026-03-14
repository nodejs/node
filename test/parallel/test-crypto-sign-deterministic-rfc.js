'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 2))
  common.skip('deterministic nonce requires OpenSSL >= 3.2');

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');

const vectors = require(fixtures.path('crypto', 'rfc6979-vectors.json'));

const curves = new Set(crypto.getCurves());

// RFC curve names to OpenSSL curve names.
const curveNames = {
  'P-192': 'prime192v1',
  'P-224': 'secp224r1',
  'P-256': 'prime256v1',
  'P-384': 'secp384r1',
  'P-521': 'secp521r1',
  'K-163': 'sect163k1',
  'K-233': 'sect233k1',
  'K-283': 'sect283k1',
  'K-409': 'sect409k1',
  'K-571': 'sect571k1',
  'B-163': 'sect163r2',
  'B-233': 'sect233r1',
  'B-283': 'sect283r1',
  'B-409': 'sect409r1',
  'B-571': 'sect571r1',
};

// Pre-encoded curve OID TLVs (tag 0x06 + length + OID bytes).
const curveOids = {
  'prime192v1': '06082a8648ce3d030101',
  'secp224r1': '06052b81040021',
  'prime256v1': '06082a8648ce3d030107',
  'secp384r1': '06052b81040022',
  'secp521r1': '06052b81040023',
  'sect163k1': '06052b81040001',
  'sect233k1': '06052b8104001a',
  'sect283k1': '06052b81040010',
  'sect409k1': '06052b81040024',
  'sect571k1': '06052b81040026',
  'sect163r2': '06052b8104000f',
  'sect233r1': '06052b8104001b',
  'sect283r1': '06052b81040011',
  'sect409r1': '06052b81040025',
  'sect571r1': '06052b81040027',
};

// ASN.1 DER helpers.
function derLength(len) {
  if (len < 0x80) return Buffer.from([len]);
  if (len < 0x100) return Buffer.from([0x81, len]);
  return Buffer.from([0x82, (len >> 8) & 0xff, len & 0xff]);
}

function derWrap(tag, content) {
  return Buffer.concat([Buffer.from([tag]), derLength(content.length), content]);
}

function derInteger(hex) {
  // Pad odd-length hex strings.
  if (hex.length % 2) hex = '0' + hex;
  let bytes = Buffer.from(hex, 'hex');
  // ASN.1 INTEGER is signed; prepend 0x00 if high bit is set.
  if (bytes[0] & 0x80) {
    bytes = Buffer.concat([Buffer.from([0x00]), bytes]);
  }
  return derWrap(0x02, bytes);
}

// Build a SEC1 ECPrivateKey DER from a raw private key hex and curve name.
//   ECPrivateKey ::= SEQUENCE {
//     version        INTEGER { ecPrivkeyVer1(1) },
//     privateKey     OCTET STRING,
//     parameters [0] ECParameters }
function buildECKey(opensslCurve, privateKeyHex) {
  // Pad odd-length hex strings.
  if (privateKeyHex.length % 2) privateKeyHex = '0' + privateKeyHex;
  const pkBytes = Buffer.from(privateKeyHex, 'hex');
  const version = Buffer.from([0x02, 0x01, 0x01]); // INTEGER 1
  const pk = derWrap(0x04, pkBytes); // OCTET STRING
  const oidTlv = Buffer.from(curveOids[opensslCurve], 'hex');
  const params = derWrap(0xa0, oidTlv); // [0] EXPLICIT
  const der = derWrap(0x30, Buffer.concat([version, pk, params]));
  return crypto.createPrivateKey({ key: der, format: 'der', type: 'sec1' });
}

// Build a traditional DSA private key DER.
//   DSAPrivateKey ::= SEQUENCE {
//     version  INTEGER,
//     p        INTEGER,
//     q        INTEGER,
//     g        INTEGER,
//     y        INTEGER,  -- public key: g^x mod p
//     x        INTEGER }
function buildDSAKey(pHex, qHex, gHex, xHex) {
  // Compute y = g^x mod p using BigInt.
  const pBig = BigInt('0x' + pHex);
  const gBig = BigInt('0x' + gHex);
  const xBig = BigInt('0x' + xHex);
  let base = gBig % pBig;
  let exp = xBig;
  let result = 1n;
  while (exp > 0n) {
    if (exp & 1n) result = (result * base) % pBig;
    exp >>= 1n;
    base = (base * base) % pBig;
  }
  const yHex = result.toString(16);

  const der = derWrap(0x30, Buffer.concat([
    derInteger('00'), // version
    derInteger(pHex),
    derInteger(qHex),
    derInteger(gHex),
    derInteger(yHex),
    derInteger(xHex),
  ]));

  const pem = '-----BEGIN DSA PRIVATE KEY-----\n' +
    der.toString('base64').match(/.{1,64}/g).join('\n') + '\n' +
    '-----END DSA PRIVATE KEY-----\n';
  return crypto.createPrivateKey(pem);
}

// Pad a hex string to an even number of chars, then to a specific byte length.
function padHex(hex, byteLen) {
  if (hex.length % 2) hex = '0' + hex;
  return hex.padStart(byteLen * 2, '0');
}

for (const section of vectors) {
  if (section.type === 'DSA') {
    // DSA sections.
    let key;
    try {
      key = buildDSAKey(section.p, section.q, section.g, section.x);
    } catch {
      common.printSkipMessage(`${section.section} (${section.title}): unsupported`);
      continue;
    }

    for (const sig of section.signatures) {
      const message = Buffer.from(sig.message);
      const signed = crypto.sign(sig.hash, message, {
        key,
        dsaNonceType: 'deterministic',
        dsaEncoding: 'ieee-p1363',
      });

      // Each component is ceil(qBits/8) bytes.
      const halfLen = signed.length / 2;
      const rGot = signed.subarray(0, halfLen).toString('hex');
      const sGot = signed.subarray(halfLen).toString('hex');
      const rExp = padHex(sig.r, halfLen);
      const sExp = padHex(sig.s, halfLen);

      assert.strictEqual(rGot, rExp.toLowerCase(),
                         `${section.section} ${sig.hash}/${sig.message} r mismatch`);
      assert.strictEqual(sGot, sExp.toLowerCase(),
                         `${section.section} ${sig.hash}/${sig.message} s mismatch`);
    }
  } else {
    // ECDSA sections.
    const opensslCurve = curveNames[section.curve];
    if (!opensslCurve || !curves.has(opensslCurve)) {
      common.printSkipMessage(`${section.section} (${section.title}): curve ${section.curve} unavailable`);
      continue;
    }

    let key;
    try {
      key = buildECKey(opensslCurve, section.x);
    } catch {
      common.printSkipMessage(`${section.section} (${section.title}): key creation failed`);
      continue;
    }

    for (const sig of section.signatures) {
      const message = Buffer.from(sig.message);
      const signed = crypto.sign(sig.hash, message, {
        key,
        dsaNonceType: 'deterministic',
        dsaEncoding: 'ieee-p1363',
      });

      const halfLen = signed.length / 2;
      const rGot = signed.subarray(0, halfLen).toString('hex');
      const sGot = signed.subarray(halfLen).toString('hex');
      const rExp = padHex(sig.r, halfLen);
      const sExp = padHex(sig.s, halfLen);

      assert.strictEqual(rGot, rExp.toLowerCase(),
                         `${section.section} ${sig.hash}/${sig.message} r mismatch`);
      assert.strictEqual(sGot, sExp.toLowerCase(),
                         `${section.section} ${sig.hash}/${sig.message} s mismatch`);
    }
  }
}
