'use strict';

const {
  hasOpenSSL,
  hasFIPS,
} = require('../../common/crypto.js');

const conditionalFileSkips = {};
const conditionalSubtestSkips = {};

function skip(...files) {
  for (const file of files) {
    const provider = process.features.openssl_is_boringssl ?
      'BoringSSL' :
      `OpenSSL ${process.versions.openssl}${hasFIPS(3) ? ' FIPS mode' : ''}`;
    conditionalFileSkips[file] = {
      'skip': `Unsupported in ${provider}`,
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

if (!hasOpenSSL(3, 2) || hasFIPS(3)) {
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
    ['getPublicKey.tentative.https.any.js', /ml-(?:kem|dsa)/i],
    [
      'supports-modern.tentative.https.any.js',
      /(?:ml-(?:kem|dsa)|(?:en|de)capsulateKey)/i,
    ]);
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
    ['getPublicKey.tentative.https.any.js', /(?:ed448|x448|ml-kem-512)/i],
    ['import_export/ML-KEM_importKey.tentative.https.any.js', /ml-kem-512/i],
    ['serialization/mlkem.tentative.https.any.js', /ml-kem-512/i],
    ['supports-modern.tentative.https.any.js', /ml-kem-512/i]);
}

if (hasFIPS(3)) {
  skip(
    'encrypt_decrypt/aes_ocb.tentative.https.any.js',
    'encrypt_decrypt/chacha20_poly1305.tentative.https.any.js',
    'generateKey/failures_chacha20_poly1305.tentative.https.any.js',
    'generateKey/successes_chacha20_poly1305.tentative.https.any.js',
    'import_export/ChaCha20-Poly1305_importKey.tentative.https.any.js',
    'serialization/chacha20-poly1305.tentative.https.any.js');

  skipSubtests(
    [
      'supports-modern.tentative.https.any.js',
      /(?:ChaCha20-Poly1305|^supports returns (?:true|false) for algorithm objects with (?:valid|invalid) parameters$)/,
    ],
    [
      'wrapKey_unwrapKey/wrapKey_unwrapKey.https.any.js',
      /(?=.*(?:RSASSA-PKCS1-v1_5|RSA-PSS|RSA-OAEP) private key)(?=.*non-extractable)/,
    ]);
}

if (hasFIPS()) {
  skip(
    'digest/kangarootwelve.tentative.https.any.js',
    'digest/turboshake.tentative.https.any.js');
}

// OpenSSL 3.0 through 3.3 reject SHA-1 signature generation in FIPS mode.
// OpenSSL 3.4 permits it for legacy use cases while marking the operation as
// non-approved through a per-operation FIPS indicator. Node does not expose
// that indicator, so the round-trip tests succeed.
if (hasFIPS(3) && !hasOpenSSL(3, 4)) {
  skipSubtests(
    ['sign_verify/ecdsa.https.any.js', /with SHA-1.*round trip$/],
    ['sign_verify/rsa_pkcs.https.any.js', /with SHA-1.*round trip$/],
    ['sign_verify/rsa_pss.https.any.js', /with SHA-1.*round trip$/]);
}

if (hasFIPS(3, 5)) {
  skip(
    'derive_bits_keys/cfrg_curves_bits_curve25519.https.any.js',
    'derive_bits_keys/cfrg_curves_bits_curve448.tentative.https.any.js',
    'derive_bits_keys/cfrg_curves_keys_curve25519.https.any.js',
    'derive_bits_keys/cfrg_curves_keys_curve448.tentative.https.any.js',
    'generateKey/successes_X25519.https.any.js',
    'generateKey/successes_X448.tentative.https.any.js',
    'import_export/okp_importKey_X25519.https.any.js',
    'import_export/okp_importKey_X448.tentative.https.any.js',
    'import_export/okp_importKey_failures_X25519.https.any.js',
    'import_export/okp_importKey_failures_X448.tentative.https.any.js',
    'serialization/x25519.https.any.js',
    'serialization/x448.tentative.https.any.js');

  skipSubtests(
    [
      'derive_bits_keys/derived_bits_length.https.any.js',
      /^X25519 derivation/,
    ],
    ['getPublicKey.tentative.https.any.js', /(?:X25519|X448)/],
    [
      'import_export/raw_format_aliases.tentative.https.any.js',
      /(?:X25519|X448)/,
    ],
    [
      'supports.tentative.https.any.js',
      /(?:X25519|^deriveKey promise tests|^supports validates the ECDH public key$)/,
    ],
    [
      'wrapKey_unwrapKey/wrapKey_unwrapKey.https.any.js',
      /(?=.*(?:X25519|X448))(?=.*(?:jwk|as non-extractable using pkcs8))/,
    ]);
}

if (hasFIPS(4)) {
  skipSubtests(
    [
      'derive_bits_keys/pbkdf2.https.any.js',
      /(?:empty password|(?:short|empty) salt|with 1 iterations)/,
    ]);
}

if (!hasFIPS()) {
  skipSubtests(
    ['digest/kangarootwelve.tentative.https.any.js', /C=(?:\d{4,}|5(?:1[3-9]|[2-9]\d)|[6-9]\d{2}) bytes/]);
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
};
