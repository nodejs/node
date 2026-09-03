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

['ML-DSA-44', 'ML-DSA-65', 'ML-DSA-87'].forEach(name => {
  ['sign', 'verify'].forEach(operation => {
    test(() => {
      assert_true(
        SubtleCrypto.supports(operation, {
          name,
          context: new Uint8Array(255),
        }),
        `${name} ${operation} supports a 255-byte context`
      );
      assert_false(
        SubtleCrypto.supports(operation, {
          name,
          context: new Uint8Array(256),
        }),
        `${name} ${operation} rejects a 256-byte context`
      );
    }, `supports validates ${name} ${operation} context length`);
  });
});

const mlKemImportedKeyCases = [
  {
    description: 'a matching HMAC length',
    additionalAlgorithm: {name: 'HMAC', hash: 'SHA-256', length: 256},
    expected: true,
  },
  {
    description: 'an algorithm without raw-secret import',
    additionalAlgorithm: 'Ed25519',
    expected: false,
  },
  {
    description: 'an HMAC length shorter than the shared secret',
    additionalAlgorithm: {name: 'HMAC', hash: 'SHA-256', length: 128},
    expected: false,
  },
  {
    description: 'an HMAC length longer than the shared secret',
    additionalAlgorithm: {name: 'HMAC', hash: 'SHA-256', length: 512},
    expected: false,
  },
];

['encapsulateKey', 'decapsulateKey'].forEach(operation => {
  mlKemImportedKeyCases.forEach(({
    description,
    additionalAlgorithm,
    expected,
  }) => {
    test(() => {
      assert_equals(
        SubtleCrypto.supports(
          operation,
          'ML-KEM-768',
          additionalAlgorithm
        ),
        expected,
        `ML-KEM-768 ${operation} with ${description}`
      );
    }, `supports ${operation} with ${description}`);
  });
});

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
