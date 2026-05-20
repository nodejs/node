'use strict';

const os = require('node:os');

const { hasOpenSSL } = require('../../common/crypto.js');

const s390x = os.arch() === 's390x';

const conditionalFileSkips = {};
const conditionalSubtestSkips = {};

function skip(...files) {
  for (const file of files) {
    conditionalFileSkips[file] = {
      'skip': 'Unsupported in ' + (process.features.openssl_is_boringssl ? 'BoringSSL' : `OpenSSL ${process.versions.openssl}`),
    };
  }
}

function skipSubtests(...entries) {
  for (const [file, regexp] of entries) {
    conditionalSubtestSkips[file] ||= {
      'skipTests': [],
    };

    conditionalSubtestSkips[file].skipTests.push(regexp);
  }
}

if (!hasOpenSSL(3, 0)) {
  skip(
    'encrypt_decrypt/aes_ocb.tentative.https.any.js',
    'generateKey/failures_AES-OCB.tentative.https.any.js',
    'generateKey/failures_kmac.tentative.https.any.js',
    'generateKey/successes_AES-OCB.tentative.https.any.js',
    'generateKey/successes_kmac.tentative.https.any.js',
    'import_export/AES-OCB_importKey.tentative.https.any.js',
    'import_export/KMAC_importKey.tentative.https.any.js',
    'sign_verify/kmac.tentative.https.any.js');
}

if (!hasOpenSSL(3, 2)) {
  skip(
    'derive_bits_keys/argon2.tentative.https.any.js',
    'import_export/Argon2_importKey.tentative.https.any.js');
}

if (!hasOpenSSL(3, 5) && !process.features.openssl_is_boringssl) {
  skip(
    'encap_decap/encap_decap_bits.tentative.https.any.js',
    'encap_decap/encap_decap_keys.tentative.https.any.js',
    'generateKey/failures_ML-DSA.tentative.https.any.js',
    'generateKey/failures_ML-KEM.tentative.https.any.js',
    'generateKey/successes_ML-DSA.tentative.https.any.js',
    'generateKey/successes_ML-KEM.tentative.https.any.js',
    'import_export/ML-DSA_importKey.tentative.https.any.js',
    'import_export/ML-KEM_importKey.tentative.https.any.js',
    'sign_verify/mldsa.tentative.https.any.js');

  skipSubtests(
    ['supports-modern.tentative.https.any.js', /ml-(?:kem|dsa)/i]);
}

if (process.features.openssl_is_boringssl) {
  skip(
    'derive_bits_keys/cfrg_curves_bits_curve448.tentative.https.any.js',
    'derive_bits_keys/cfrg_curves_keys_curve448.tentative.https.any.js',
    'digest/cshake.tentative.https.any.js',
    'digest/sha3.tentative.https.any.js',
    'generateKey/failures_Ed448.tentative.https.any.js',
    'generateKey/failures_X448.tentative.https.any.js',
    'generateKey/successes_Ed448.tentative.https.any.js',
    'generateKey/successes_X448.tentative.https.any.js',
    'import_export/okp_importKey_Ed448.tentative.https.any.js',
    'import_export/okp_importKey_failures_Ed448.tentative.https.any.js',
    'import_export/okp_importKey_failures_X448.tentative.https.any.js',
    'import_export/okp_importKey_X448.tentative.https.any.js',
    'sign_verify/eddsa_curve448.tentative.https.any.js');

  skipSubtests(
    ['encap_decap/encap_decap_bits.tentative.https.any.js', /ml-kem-512/i],
    ['encap_decap/encap_decap_keys.tentative.https.any.js', /ml-kem-512/i],
    ['generateKey/failures_ML-KEM.tentative.https.any.js', /ml-kem-512/i],
    ['generateKey/successes_ML-KEM.tentative.https.any.js', /ml-kem-512/i],
    ['import_export/ML-KEM_importKey.tentative.https.any.js', /ml-kem-512/i],
    ['supports-modern.tentative.https.any.js', /ml-kem-512/i]);
}

function assertNoOverlap(fileSkips, subtestSkips) {
  const subtestSkipFiles = new Set(Object.keys(subtestSkips));
  const overlap = Object.keys(fileSkips).filter((file) => subtestSkipFiles.has(file));

  if (overlap.length !== 0) {
    throw new Error(`conditionalFileSkips and conditionalSubtestSkips overlap: ${overlap.join(', ')}`);
  }
}

assertNoOverlap(conditionalFileSkips, conditionalSubtestSkips);

module.exports = {
  ...conditionalFileSkips,
  ...conditionalSubtestSkips,
  'algorithm-discards-context.https.window.js': {
    'skip': 'Not relevant in Node.js context',
  },
  'historical.any.js': {
    'skip': 'Not relevant in Node.js context',
  },
  'sign_verify/eddsa_small_order_points.https.any.js': {
    'fail': {
      'note': 'see https://github.com/nodejs/node/issues/54572',
      'expected': [
        'Ed25519 Verification checks with small-order key of order - Test 1',
        'Ed25519 Verification checks with small-order key of order - Test 2',
        'Ed25519 Verification checks with small-order key of order - Test 12',
        'Ed25519 Verification checks with small-order key of order - Test 13',
        ...(s390x ? [] : [
          'Ed25519 Verification checks with small-order key of order - Test 0',
          'Ed25519 Verification checks with small-order key of order - Test 11',
        ]),
      ],
    },
  },
};
