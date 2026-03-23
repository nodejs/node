'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(4))
  common.skip('requires OpenSSL >= 4.0 for cSHAKE functionName support');

const assert = require('assert');
const crypto = require('crypto');
const { subtle } = globalThis.crypto;

const vectors = require('../fixtures/crypto/kmac')();

// This test cross-checks two independent WebCrypto/OpenSSL paths:
//
// * KMAC sign() uses OpenSSL's KMAC MAC implementation.
// * cSHAKE digest() uses OpenSSL's cSHAKE digest implementation, with
//   functionName="KMAC" and a manually encoded KMAC message.
//
// NIST SP 800-185 defines KMAC in terms of cSHAKE. If the cSHAKE
// functionName/customization parameters are wired or encoded incorrectly,
// this identity stops holding even when standalone KMAC vectors still pass.
// The assertions below therefore protect the cSHAKE parameter plumbing
// against regressions by requiring both operations to produce the same NIST
// sample vectors. This is not broad randomized coverage; it checks the
// specification identity against the published KMAC examples.
//
// NIST SP 800-185 encoding primitives used to construct KMAC input from
// its components so we can cross-check against cSHAKE with functionName="KMAC".
//
// KMAC128(K, X, L, S) =
//   cSHAKE128(bytepad(encode_string(K), 168) || X || right_encode(L),
//             L, "KMAC", S)
// KMAC256(K, X, L, S) =
//   cSHAKE256(bytepad(encode_string(K), 136) || X || right_encode(L),
//             L, "KMAC", S)

// left_encode(x): big-endian encoding of x prefixed with the byte count.
function leftEncode(x) {
  if (x === 0) return Buffer.from([1, 0]);
  const bytes = [];
  let v = x;
  while (v > 0) {
    bytes.unshift(v & 0xff);
    v = Math.floor(v / 256);
  }
  return Buffer.from([bytes.length, ...bytes]);
}

// right_encode(x): big-endian encoding of x suffixed with the byte count.
function rightEncode(x) {
  if (x === 0) return Buffer.from([0, 1]);
  const bytes = [];
  let v = x;
  while (v > 0) {
    bytes.unshift(v & 0xff);
    v = Math.floor(v / 256);
  }
  return Buffer.from([...bytes, bytes.length]);
}

// encode_string(S) = left_encode(len(S) * 8) || S
function encodeString(s) {
  return Buffer.concat([leftEncode(s.length * 8), s]);
}

// bytepad(X, w) = left_encode(w) || X || 0*  (padded to multiple of w)
function bytepad(x, w) {
  const prefix = leftEncode(w);
  const z = Buffer.concat([prefix, x]);
  const padLen = w - (z.length % w);
  if (padLen === w) return z;
  return Buffer.concat([z, Buffer.alloc(padLen)]);
}

// Build the full cSHAKE input corresponding to KMAC(K, X, L, S):
//   bytepad(encode_string(K), rate) || X || right_encode(L)
function buildKmacCshakeInput(key, data, outputLengthBits, rate) {
  return Buffer.concat([
    bytepad(encodeString(key), rate),
    data,
    rightEncode(outputLengthBits),
  ]);
}

const encode = (str) => new TextEncoder().encode(str);

function randomNonZeroBytes(length) {
  const bytes = crypto.randomBytes(length);
  for (let n = 0; n < bytes.length; n++) {
    if (bytes[n] === 0) bytes[n] = 1;
  }
  return bytes;
}

(async () => {
  for (const { algorithm, key, data, customization, outputLength, expected } of vectors) {
    const cshakeName = algorithm === 'KMAC128' ? 'cSHAKE128' : 'cSHAKE256';
    // These are the byte rates from SP 800-185 for the underlying Keccak
    // sponge: cSHAKE128 uses rate 168, cSHAKE256 uses rate 136.
    const rate = algorithm === 'KMAC128' ? 168 : 136;

    // Reconstruct the exact message that KMAC feeds into cSHAKE. The cSHAKE
    // operation itself receives functionName="KMAC" and the same optional
    // customization string that was supplied to KMAC.
    const cshakeInput = buildKmacCshakeInput(key, data, outputLength, rate);

    const cshakeParams = {
      name: cshakeName,
      outputLength,
      functionName: encode('KMAC'),
    };

    if (customization !== undefined) {
      cshakeParams.customization = customization;
    }

    const [kmacResult, cshakeResult] = await Promise.all([
      // Exercise the WebCrypto KMAC path directly.
      (async () => {
        const kmacKey = await subtle.importKey(
          'raw-secret', key, { name: algorithm }, false, ['sign']);
        return subtle.sign(
          { name: algorithm, outputLength, customization }, kmacKey, data);
      })(),
      // Exercise the WebCrypto cSHAKE path using the SP 800-185 reduction of
      // KMAC to cSHAKE. This is the path that validates cSHAKE function-name
      // handling.
      subtle.digest(cshakeParams, cshakeInput),
    ]);

    // Both routes must match the same published vector. A mismatch in only the
    // cSHAKE assertion points to the functionName/customization bridge rather
    // than the KMAC fixture itself.
    assert.deepStrictEqual(
      Buffer.from(kmacResult),
      expected,
      `${algorithm} KMAC sign result mismatch`,
    );
    assert.deepStrictEqual(
      Buffer.from(cshakeResult),
      expected,
      `${algorithm} cSHAKE cross-check mismatch for vector with ` +
      `${customization ? `customization="${customization}"` : 'no customization'}`,
    );
  }

  // Add one runtime-generated case that is not tied to the published NIST
  // samples. It still checks only the SP 800-185 identity between KMAC and
  // cSHAKE, but with fresh input sizes and contents on each run.
  {
    const algorithm = 'KMAC128';
    const key = crypto.randomBytes(37);
    const data = crypto.randomBytes(113);
    // Avoid NUL here because OpenSSL's cSHAKE UTF8 parameter handling has
    // separate NUL-specific behavior; this case is for non-fixture coverage.
    const customization = randomNonZeroBytes(29);
    const outputLength = 384;
    const cshakeInput = buildKmacCshakeInput(key, data, outputLength, 168);

    const kmacKey = await subtle.importKey(
      'raw-secret', key, { name: algorithm }, false, ['sign']);
    const [kmacResult, cshakeResult] = await Promise.all([
      subtle.sign(
        { name: algorithm, outputLength, customization }, kmacKey, data),
      subtle.digest({
        name: 'cSHAKE128',
        outputLength,
        functionName: encode('KMAC'),
        customization,
      }, cshakeInput),
    ]);

    assert.deepStrictEqual(
      Buffer.from(cshakeResult),
      Buffer.from(kmacResult),
      `${algorithm} random cSHAKE cross-check mismatch`,
    );
  }
})().then(common.mustCall());
