// META: title=WebCrypto API: supports method tests for algorithms in https://wicg.github.io/webcrypto-modern-algos/
// META: script=util/helpers.js
// META: script=util/supports.js

'use strict';

const modernAlgorithms = {
  // Asymmetric algorithms
  'ML-DSA-44': {
    operations: ['generateKey', 'importKey', 'sign', 'verify', 'getPublicKey'],
  },
  'ML-DSA-65': {
    operations: ['generateKey', 'importKey', 'sign', 'verify', 'getPublicKey'],
  },
  'ML-DSA-87': {
    operations: ['generateKey', 'importKey', 'sign', 'verify', 'getPublicKey'],
  },
  'ML-KEM-512': {
    operations: [
      'generateKey', 'importKey', 'encapsulateKey', 'encapsulateBits',
      'decapsulateKey', 'decapsulateBits', 'getPublicKey'
    ],
  },
  'ML-KEM-768': {
    operations: [
      'generateKey', 'importKey', 'encapsulateKey', 'encapsulateBits',
      'decapsulateKey', 'decapsulateBits', 'getPublicKey'
    ],
  },
  'ML-KEM-1024': {
    operations: [
      'generateKey', 'importKey', 'encapsulateKey', 'encapsulateBits',
      'decapsulateKey', 'decapsulateBits', 'getPublicKey'
    ],
  },

  // Symmetric algorithms
  'ChaCha20-Poly1305': {
    operations: ['generateKey', 'importKey', 'encrypt', 'decrypt'],
    encryptParams: {name: 'ChaCha20-Poly1305', iv: new Uint8Array(12)},
  },

};

const operations = [
  'generateKey',
  'importKey',
  'sign',
  'verify',
  'encrypt',
  'decrypt',
  'deriveBits',
  'digest',
  'encapsulateKey',
  'encapsulateBits',
  'decapsulateKey',
  'decapsulateBits',
  'getPublicKey',
];

// Test that supports method exists and is a static method
testSupportsMethod();


// Test standard WebCrypto algorithms for requested operations
runSupportsTests(modernAlgorithms, operations);

// Test some algorithm objects with valid parameters
test(() => {
  assert_true(
      SubtleCrypto.supports('encrypt', {
        name: 'ChaCha20-Poly1305',
        iv: new Uint8Array(12),
        tagLength: 128,
      }),
      'ChaCha20-Poly1305 encrypt with valid tagLength');
}, 'supports returns true for algorithm objects with valid parameters');

// Test some algorithm objects with invalid parameters
test(() => {
  assert_false(
      SubtleCrypto.supports('encrypt', {
        name: 'ChaCha20-Poly1305',
        iv: new Uint8Array(12),
        tagLength: 100,
      }),
      'ChaCha20-Poly1305 encrypt with invalid tagLength');

  assert_false(
      SubtleCrypto.supports('encrypt', {
        name: 'ChaCha20-Poly1305',
        iv: new Uint8Array(10),
        tagLength: 128,
      }),
      'ChaCha20-Poly1305 encrypt with invalid iv');
}, 'supports returns false for algorithm objects with invalid parameters');


done();
