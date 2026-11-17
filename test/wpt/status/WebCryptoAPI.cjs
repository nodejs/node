'use strict';

const {
  hasOpenSSL,
  hasFIPS,
  isBoringSSL,
} = require('../../common/crypto.js');

const conditionalFileSkips = {};
const conditionalSubtestSkips = {};

function skip(...files) {
  for (const file of files) {
    const provider = isBoringSSL ?
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

if (isBoringSSL) {
  skip(
    'modern-algos/encrypt_decrypt/aes_ocb.https.any.js',
    'modern-algos/generateKey/failures_AES-OCB.https.any.js',
    'modern-algos/generateKey/failures_kmac.https.any.js',
    'modern-algos/generateKey/successes_AES-OCB.https.any.js',
    'modern-algos/generateKey/successes_kmac.https.any.js',
    'modern-algos/import_export/AES-OCB_importKey.https.any.js',
    'modern-algos/import_export/KMAC_importKey.https.any.js',
    'modern-algos/serialization/aes-ocb.https.any.js',
    'modern-algos/serialization/kmac.https.any.js',
    'modern-algos/sign_verify/kmac.https.any.js');
}

if (!hasOpenSSL(3, 2) || hasFIPS(3)) {
  skip(
    'modern-algos/derive_bits_keys/argon2.https.any.js',
    'modern-algos/import_export/Argon2_importKey.https.any.js');
}

if (!hasOpenSSL(3, 5) && !isBoringSSL) {
  skip(
    'modern-algos/encap_decap/encap_decap_bits.https.any.js',
    'modern-algos/encap_decap/encap_decap_keys.https.any.js',
    'modern-algos/generateKey/failures_Hybrid-KEM.https.any.js',
    'modern-algos/generateKey/failures_ML-DSA.https.any.js',
    'modern-algos/generateKey/failures_ML-KEM.https.any.js',
    'modern-algos/generateKey/successes_Hybrid-KEM.https.any.js',
    'modern-algos/generateKey/successes_ML-DSA.https.any.js',
    'modern-algos/generateKey/successes_ML-KEM.https.any.js',
    'modern-algos/import_export/Hybrid-KEM_importKey.https.any.js',
    'modern-algos/import_export/ML-DSA_importKey.https.any.js',
    'modern-algos/import_export/ML-KEM_importKey.https.any.js',
    'modern-algos/serialization/hybridkem.https.window.js',
    'modern-algos/serialization/mldsa.https.any.js',
    'modern-algos/serialization/mlkem.https.any.js',
    'modern-algos/sign_verify/mldsa.https.any.js');

  skipSubtests(
    ['modern-algos/getPublicKey.https.any.js', /ml-?(?:kem|dsa)/i],
    [
      'modern-algos/supports-modern.https.any.js',
      /(?:ml-?(?:kem|dsa)|(?:en|de)capsulateKey)/i,
    ]);
}

if (isBoringSSL) {
  skip(
    'secure-curves/derive_bits_keys/cfrg_curves_bits_curve448.https.any.js',
    'secure-curves/derive_bits_keys/cfrg_curves_keys_curve448.https.any.js',
    'modern-algos/digest/cshake.https.any.js',
    'secure-curves/generateKey/failures_Ed448.https.any.js',
    'modern-algos/generateKey/failures_Hybrid-KEM.https.any.js',
    'secure-curves/generateKey/failures_X448.https.any.js',
    'secure-curves/generateKey/successes_Ed448.https.any.js',
    'modern-algos/generateKey/successes_Hybrid-KEM.https.any.js',
    'secure-curves/generateKey/successes_X448.https.any.js',
    'modern-algos/import_export/Hybrid-KEM_importKey.https.any.js',
    'secure-curves/import_export/okp_importKey_Ed448.https.any.js',
    'secure-curves/import_export/okp_importKey_failures_Ed448.https.any.js',
    'secure-curves/import_export/okp_importKey_failures_X448.https.any.js',
    'secure-curves/import_export/okp_importKey_X448.https.any.js',
    'secure-curves/serialization/ed448.https.any.js',
    'modern-algos/serialization/hybridkem.https.window.js',
    'secure-curves/serialization/x448.https.any.js',
    'secure-curves/sign_verify/eddsa_curve448.https.any.js');

  skipSubtests(
    ['modern-algos/encap_decap/encap_decap_bits.https.any.js', /(?:ml-kem-512|mlkem)/i],
    ['modern-algos/encap_decap/encap_decap_keys.https.any.js', /(?:ml-kem-512|mlkem)/i],
    ['modern-algos/generateKey/failures_ML-KEM.https.any.js', /ml-kem-512/i],
    ['modern-algos/generateKey/successes_ML-KEM.https.any.js', /ml-kem-512/i],
    ['modern-algos/getPublicKey.https.any.js', /(?:ed448|x448|ml-kem-512|mlkem)/i],
    ['modern-algos/import_export/ML-KEM_importKey.https.any.js', /ml-kem-512/i],
    ['modern-algos/serialization/mlkem.https.any.js', /ml-kem-512/i],
    ['modern-algos/supports-modern.https.any.js', /(?:ml-kem-512|mlkem)/i]);
}

if (hasFIPS(3)) {
  skip(
    'modern-algos/encrypt_decrypt/aes_ocb.https.any.js',
    'modern-algos/encrypt_decrypt/chacha20_poly1305.https.any.js',
    'modern-algos/generateKey/failures_AES-OCB.https.any.js',
    'modern-algos/generateKey/failures_chacha20_poly1305.https.any.js',
    'modern-algos/generateKey/successes_AES-OCB.https.any.js',
    'modern-algos/generateKey/successes_chacha20_poly1305.https.any.js',
    'modern-algos/import_export/AES-OCB_importKey.https.any.js',
    'modern-algos/import_export/ChaCha20-Poly1305_importKey.https.any.js',
    'modern-algos/serialization/aes-ocb.https.any.js',
    'modern-algos/serialization/chacha20-poly1305.https.any.js');

  skipSubtests(
    [
      'modern-algos/supports-modern.https.any.js',
      /(?:ChaCha20-Poly1305|^supports returns (?:true|false) for algorithm objects with (?:valid|invalid) parameters$)/,
    ],
    [
      'wrapKey_unwrapKey/wrapKey_unwrapKey.https.any.js',
      /(?=.*(?:RSASSA-PKCS1-v1_5|RSA-PSS|RSA-OAEP) private key)(?=.*non-extractable)/,
    ]);
}

if (hasFIPS()) {
  skip(
    'modern-algos/digest/kangarootwelve.https.any.js',
    'modern-algos/digest/turboshake.https.any.js');
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
    'secure-curves/derive_bits_keys/cfrg_curves_bits_curve448.https.any.js',
    'derive_bits_keys/cfrg_curves_keys_curve25519.https.any.js',
    'secure-curves/derive_bits_keys/cfrg_curves_keys_curve448.https.any.js',
    'modern-algos/generateKey/failures_Hybrid-KEM.https.any.js',
    'generateKey/failures_X25519.https.any.js',
    'secure-curves/generateKey/failures_X448.https.any.js',
    'modern-algos/generateKey/successes_Hybrid-KEM.https.any.js',
    'generateKey/successes_X25519.https.any.js',
    'secure-curves/generateKey/successes_X448.https.any.js',
    'modern-algos/import_export/Hybrid-KEM_importKey.https.any.js',
    'import_export/okp_importKey_X25519.https.any.js',
    'secure-curves/import_export/okp_importKey_X448.https.any.js',
    'import_export/okp_importKey_failures_X25519.https.any.js',
    'secure-curves/import_export/okp_importKey_failures_X448.https.any.js',
    'modern-algos/serialization/hybridkem.https.window.js',
    'serialization/x25519.https.any.js',
    'secure-curves/serialization/x448.https.any.js');

  skipSubtests(
    [
      'derive_bits_keys/derived_bits_length.https.any.js',
      /^X25519 derivation/,
    ],
    ['import_export/okp_fixed_length.https.any.js', /^X25519 /],
    ['import_export/okp_malformed_asn1.https.any.js', /^X25519 /],
    ['modern-algos/encap_decap/encap_decap_bits.https.any.js', /mlkem/i],
    ['modern-algos/encap_decap/encap_decap_keys.https.any.js', /mlkem/i],
    ['modern-algos/getPublicKey.https.any.js', /(?:X25519|X448|MLKEM)/],
    [
      'modern-algos/import_export/raw_format_aliases.https.any.js',
      /(?:X25519|X448)/,
    ],
    ['modern-algos/supports-modern.https.any.js', /mlkem/i],
    [
      'modern-algos/supports.https.any.js',
      /(?:X25519|^deriveKey promise tests|^supports validates the ECDH public key$)/,
    ],
    [
      'wrapKey_unwrapKey/wrapKey_unwrapKey.https.any.js',
      /(?=.*(?:X25519|X448))(?=.*(?:jwk|as non-extractable using pkcs8))/,
    ]);
}

if (process.env.WPT_REPORT === undefined) {
  // Keep the full matrix in both globals when producing a WPT report. Regular
  // CI runs these expensive suites in the window global only.
  conditionalFileSkips['derive_bits_keys/pbkdf2.https.any.worker.html'] = {
    'skip': 'Expensive worker matrix runs with WPT_REPORT',
  };

  // Retain one 100000-iteration deriveKey per PBKDF2 vector; deriveBits still
  // checks every password, salt, and hash, and cheaper vectors cover all
  // derived key types.
  skipSubtests([
    'derive_bits_keys/pbkdf2.https.any.html',
    /^Derived key of type (?!name: AES-CBC length: 256\s+using\b).* using .*, SHA-(?:1|256|384|512), with 100000 iterations$/,
  ]);

  // RSA name variants are grouped in the order uppercase, lowercase, mixed.
  // Keep the uppercase group and skip variants containing only the other two.
  for (const [name, firstSkipped, last] of [
    ['RSA-PSS', 21, 31],
    ['RSASSA-PKCS1-v1_5', 21, 31],
    ['RSA-OAEP', 61, 151],
  ]) {
    conditionalFileSkips[`generateKey/successes_${name}.https.any.worker.html`] = {
      'skip': 'Expensive worker matrix runs with WPT_REPORT',
    };
    const prefix = `generateKey/successes_${name}.https.any.html?`;
    skipSubtests([
      `${prefix}${firstSkipped - 10}-${firstSkipped - 1}`,
      /name: rsa/,
    ]);
    for (let start = firstSkipped; start < last; start += 10) {
      conditionalFileSkips[`${prefix}${start}-${start + 9}`] = {
        'skip': 'Redundant RSA key generation in regular CI',
      };
    }
    conditionalFileSkips[`${prefix}${last}-last`] = {
      'skip': 'Redundant RSA key generation in regular CI',
    };
  }
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
    ['modern-algos/digest/kangarootwelve.https.any.js', /C=(?:\d{4,}|5(?:1[3-9]|[2-9]\d)|[6-9]\d{2}) bytes/]);
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
