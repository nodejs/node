// META: title=WebCrypto API: supports method tests for algorithms in https://wicg.github.io/webcrypto-modern-algos/
// META: script=util/helpers.js

'use strict';

const modernAlgorithms = {
  // Asymmetric algorithms
  'ML-DSA-44': {
    operations: ['generateKey', 'importKey', 'sign', 'verify'],
  },
  'ML-DSA-65': {
    operations: ['generateKey', 'importKey', 'sign', 'verify'],
  },
  'ML-DSA-87': {
    operations: ['generateKey', 'importKey', 'sign', 'verify'],
  },
  'ML-KEM-512': {
    operations: [
      'generateKey', 'importKey', 'encapsulateKey', 'encapsulateBits',
      'decapsulateKey', 'decapsulateBits'
    ],
  },
  'ML-KEM-768': {
    operations: [
      'generateKey', 'importKey', 'encapsulateKey', 'encapsulateBits',
      'decapsulateKey', 'decapsulateBits'
    ],
  },
  'ML-KEM-1024': {
    operations: [
      'generateKey', 'importKey', 'encapsulateKey', 'encapsulateBits',
      'decapsulateKey', 'decapsulateBits'
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
];

// Test that supports method exists and is a static method
test(() => {
  assert_true(
      typeof SubtleCrypto.supports === 'function',
      'SubtleCrypto.supports should be a function');
}, 'SubtleCrypto.supports method exists');


// Test standard WebCrypto algorithms for requested operations
for (const [algorithmName, algorithmInfo] of Object.entries(modernAlgorithms)) {
  for (const operation of operations) {
    promise_test(async (t) => {
      const isSupported = algorithmInfo.operations.includes(operation);

      // Use appropriate algorithm parameters for each operation
      let algorithm;
      let lengthOrAdditionalAlgorithm;
      switch (operation) {
        case 'generateKey':
          algorithm = algorithmInfo.keyGenParams || algorithmName;
          break;
        case 'importKey':
          algorithm = algorithmInfo.importParams || algorithmName;
          break;
        case 'sign':
        case 'verify':
          algorithm = algorithmInfo.signParams || algorithmName;
          break;
        case 'encrypt':
        case 'decrypt':
          algorithm = algorithmInfo.encryptParams || algorithmName;
          break;
        case 'deriveBits':
          algorithm = algorithmInfo.deriveBitsParams || algorithmName;
          if (algorithm?.public instanceof Promise) {
            algorithm.public = (await algorithm.public).publicKey;
          }
          if (algorithmName === 'PBKDF2' || algorithmName === 'HKDF') {
            lengthOrAdditionalAlgorithm = 256;
          }
          break;
        case 'digest':
          algorithm = algorithmName;
          break;
        case 'encapsulateKey':
        case 'encapsulateBits':
        case 'decapsulateKey':
        case 'decapsulateBits':
          algorithm = algorithmName;
          if (operation === 'encapsulateKey' || operation === 'decapsulateKey') {
            lengthOrAdditionalAlgorithm = { name: 'AES-GCM', length: 256 };
          }
          break;
        default:
          algorithm = algorithmName;
      }

      const result = SubtleCrypto.supports(operation, algorithm, lengthOrAdditionalAlgorithm);

      if (isSupported) {
        assert_true(result, `${algorithmName} should support ${operation}`);
      } else {
        assert_false(
            result, `${algorithmName} should not support ${operation}`);
      }
    }, `supports(${operation}, ${algorithmName})`);
  }
}

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
