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
    'serialization/aes-ocb.tentative.https.any.js',
    'serialization/kmac.tentative.https.any.js',
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
    'serialization/mldsa.tentative.https.any.js',
    'serialization/mlkem.tentative.https.any.js',
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
    'serialization/ed448.tentative.https.any.js',
    'serialization/x448.tentative.https.any.js',
    'sign_verify/eddsa_curve448.tentative.https.any.js');

  skipSubtests(
    ['encap_decap/encap_decap_bits.tentative.https.any.js', /ml-kem-512/i],
    ['encap_decap/encap_decap_keys.tentative.https.any.js', /ml-kem-512/i],
    ['generateKey/failures_ML-KEM.tentative.https.any.js', /ml-kem-512/i],
    ['generateKey/successes_ML-KEM.tentative.https.any.js', /ml-kem-512/i],
    ['import_export/ML-KEM_importKey.tentative.https.any.js', /ml-kem-512/i],
    ['serialization/mlkem.tentative.https.any.js', /ml-kem-512/i],
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

const cshakeExpectedFailures = ['cSHAKE128', 'cSHAKE256'].flatMap((algorithm) => {
  return [0, 256, 384, 512].flatMap((length) => {
    return ['empty', 'short', 'medium'].flatMap((size) => {
      const base = `${algorithm} with ${length} bit output and ${size} source data`;
      return [
        base,
      ].concat(size !== 'empty' ? [
        `${base} and altered buffer after call`,
        `${base} and altered buffer during call`,
        `${base} and transferred buffer after call`,
        `${base} and transferred buffer during call`,
      ] : []);
    });
  });
});

const kmacVectorNames = [
  'KMAC128 with no customization',
  'KMAC128 with customization',
  'KMAC128 with large data and customization',
  'KMAC256 with customization and 512-bit output',
  'KMAC256 with large data and no customization',
  'KMAC256 with large data and customization',
];

const kmacExpectedFailures = kmacVectorNames.flatMap((name) => {
  return [
    `${name} verification`,
    `${name} verification with transferred signature during call`,
    `${name} verification with transferred signature after call`,
    `${name} verification with altered signature during call`,
    `${name} verification with altered signature after call`,
    `${name} with altered plaintext during call`,
    `${name} with altered plaintext after call`,
    `${name} with transferred plaintext during call`,
    `${name} with transferred plaintext after call`,
    `${name} no verify usage`,
    `${name} round trip`,
    `${name} verification failure due to wrong plaintext`,
    `${name} verification failure due to wrong signature`,
    `${name} verification failure due to short signature`,
    `${name} verification failure due to wrong length parameter`,
    `${name} signing with wrong algorithm name`,
    `${name} verifying with wrong algorithm name`,
  ];
});

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
  'getRandomValues.any.js': {
    'fail': {
      'note': 'https://github.com/nodejs/node/issues/58987',
      'expected': [
        'Large length: Int8Array',
        'Large length: Int16Array',
        'Large length: Int32Array',
        'Large length: BigInt64Array',
        'Large length: Uint8Array',
        'Large length: Uint8ClampedArray',
        'Large length: Uint16Array',
        'Large length: Uint32Array',
        'Large length: BigUint64Array',
      ],
    },
  },
  'digest/cshake.tentative.https.any.js': conditionalFileSkips['digest/cshake.tentative.https.any.js'] ?? {
    'fail': {
      'note': 'WPT still uses CShakeParams.length; implementation moved to CShakeParams.outputLength',
      'expected': cshakeExpectedFailures,
    },
  },
  'sign_verify/kmac.tentative.https.any.js': conditionalFileSkips['sign_verify/kmac.tentative.https.any.js'] ?? {
    'fail': {
      'note': 'WPT still uses KmacParams.length; implementation moved to KmacParams.outputLength',
      'expected': kmacExpectedFailures,
    },
  },
};
