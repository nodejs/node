// META: title=WebCrypto API: supports method tests
// META: script=util/helpers.js

'use strict';

const standardAlgorithms = {
  // Asymmetric algorithms
  'RSASSA-PKCS1-v1_5': {
    operations: ['generateKey', 'importKey', 'sign', 'verify'],
    keyGenParams: {
      name: 'RSASSA-PKCS1-v1_5',
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256',
    },
    importParams: { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
    signParams: { name: 'RSASSA-PKCS1-v1_5' },
  },
  'RSA-PSS': {
    operations: ['generateKey', 'importKey', 'sign', 'verify'],
    keyGenParams: {
      name: 'RSA-PSS',
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256',
    },
    importParams: { name: 'RSA-PSS', hash: 'SHA-256' },
    signParams: { name: 'RSA-PSS', saltLength: 32 },
  },
  'RSA-OAEP': {
    operations: ['generateKey', 'importKey', 'encrypt', 'decrypt'],
    keyGenParams: {
      name: 'RSA-OAEP',
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256',
    },
    importParams: { name: 'RSA-OAEP', hash: 'SHA-256' },
    encryptParams: { name: 'RSA-OAEP' },
  },
  ECDSA: {
    operations: ['generateKey', 'importKey', 'sign', 'verify'],
    keyGenParams: { name: 'ECDSA', namedCurve: 'P-256' },
    importParams: { name: 'ECDSA', namedCurve: 'P-256' },
    signParams: { name: 'ECDSA', hash: 'SHA-256' },
  },
  ECDH: {
    operations: ['generateKey', 'importKey', 'deriveBits'],
    keyGenParams: { name: 'ECDH', namedCurve: 'P-256' },
    importParams: { name: 'ECDH', namedCurve: 'P-256' },
    deriveBitsParams: {
      name: 'ECDH',
      public: crypto.subtle.generateKey(
        { name: 'ECDH', namedCurve: 'P-256' },
        false,
        ['deriveBits']
      ),
    },
  },
  Ed25519: {
    operations: ['generateKey', 'importKey', 'sign', 'verify'],
    keyGenParams: null,
    signParams: { name: 'Ed25519' },
  },
  X25519: {
    operations: ['generateKey', 'importKey', 'deriveBits'],
    keyGenParams: null,
    deriveBitsParams: {
      name: 'X25519',
      public: crypto.subtle.generateKey('X25519', false, ['deriveBits']),
    },
  },

  // Symmetric algorithms
  'AES-CBC': {
    operations: ['generateKey', 'importKey', 'encrypt', 'decrypt'],
    keyGenParams: { name: 'AES-CBC', length: 256 },
    encryptParams: { name: 'AES-CBC', iv: new Uint8Array(16) },
  },
  'AES-CTR': {
    operations: ['generateKey', 'importKey', 'encrypt', 'decrypt'],
    keyGenParams: { name: 'AES-CTR', length: 256 },
    encryptParams: {
      name: 'AES-CTR',
      counter: new Uint8Array(16),
      length: 128,
    },
  },
  'AES-GCM': {
    operations: ['generateKey', 'importKey', 'encrypt', 'decrypt'],
    keyGenParams: { name: 'AES-GCM', length: 256 },
    encryptParams: { name: 'AES-GCM', iv: new Uint8Array(12) },
  },
  'AES-KW': {
    operations: ['generateKey', 'importKey'], // wrapKey/unwrapKey not in requested operations
    keyGenParams: { name: 'AES-KW', length: 256 },
  },
  HMAC: {
    operations: ['generateKey', 'importKey', 'sign', 'verify'],
    keyGenParams: { name: 'HMAC', hash: 'SHA-256' },
    importParams: { name: 'HMAC', hash: 'SHA-256' },
    signParams: { name: 'HMAC' },
  },

  // Hash algorithms
  'SHA-1': {
    operations: ['digest'],
    keyGenParams: null,
  },
  'SHA-256': {
    operations: ['digest'],
    keyGenParams: null,
  },
  'SHA-384': {
    operations: ['digest'],
    keyGenParams: null,
  },
  'SHA-512': {
    operations: ['digest'],
    keyGenParams: null,
  },

  // Key derivation algorithms
  HKDF: {
    operations: ['importKey', 'deriveBits'],
    keyGenParams: null,
    deriveBitsParams: {
      name: 'HKDF',
      hash: 'SHA-256',
      salt: new Uint8Array(16),
      info: new Uint8Array(0),
    },
  },
  PBKDF2: {
    operations: ['importKey', 'deriveBits'],
    keyGenParams: null,
    deriveBitsParams: {
      name: 'PBKDF2',
      hash: 'SHA-256',
      salt: new Uint8Array(16),
      iterations: 100000,
    },
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
];

// Test that supports method exists and is a static method
test(() => {
  assert_true(
    typeof SubtleCrypto.supports === 'function',
    'SubtleCrypto.supports should be a function'
  );
}, 'SubtleCrypto.supports method exists');

// Test invalid operation names
test(() => {
  assert_false(
    SubtleCrypto.supports('invalidOperation', 'AES-GCM'),
    'Invalid operation should return false'
  );
  assert_false(
    SubtleCrypto.supports('', 'AES-GCM'),
    'Empty operation should return false'
  );
  assert_false(
    SubtleCrypto.supports('GENERATEKEY', 'AES-GCM'),
    'Case-sensitive operation check'
  );
}, 'supports returns false for invalid operations');

// Test invalid algorithm identifiers
test(() => {
  assert_false(
    SubtleCrypto.supports('generateKey', 'InvalidAlgorithm'),
    'Invalid algorithm should return false'
  );
  assert_false(
    SubtleCrypto.supports('generateKey', ''),
    'Empty algorithm should return false'
  );
}, 'supports returns false for invalid algorithms');

// Test standard WebCrypto algorithms for requested operations
for (const [algorithmName, algorithmInfo] of Object.entries(
  standardAlgorithms
)) {
  for (const operation of operations) {
    promise_test(async (t) => {
      const isSupported = algorithmInfo.operations.includes(operation);

      // Use appropriate algorithm parameters for each operation
      let algorithm;
      let length;
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
            length = 256;
          }
          break;
        case 'digest':
          algorithm = algorithmName;
          break;
        default:
          algorithm = algorithmName;
      }

      const result = SubtleCrypto.supports(operation, algorithm, length);

      if (isSupported) {
        assert_true(result, `${algorithmName} should support ${operation}`);
      } else {
        assert_false(result, `${algorithmName} should not support ${operation}`);
      }
    }, `supports(${operation}, ${algorithmName})`);
  }
}

// Test algorithm objects (not just strings)
test(() => {
  assert_true(
    SubtleCrypto.supports('generateKey', { name: 'AES-GCM', length: 256 }),
    'Algorithm object should be supported'
  );
  assert_true(
    SubtleCrypto.supports('generateKey', { name: 'HMAC', hash: 'SHA-256' }),
    'Algorithm object with parameters should be supported'
  );
}, 'supports works with algorithm objects');

// Test with algorithm objects that have invalid parameters
test(() => {
  assert_false(
    SubtleCrypto.supports('generateKey', { name: 'AES-GCM', length: 100 }),
    'Invalid key length should return false'
  );
  assert_false(
    SubtleCrypto.supports('generateKey', {
      name: 'HMAC',
      hash: 'INVALID-HASH',
    }),
    'Invalid hash parameter should return false'
  );
}, 'supports returns false for algorithm objects with invalid parameters');

// Test some specific combinations that should work
test(() => {
  // RSA algorithms
  assert_true(
    SubtleCrypto.supports('generateKey', {
      name: 'RSASSA-PKCS1-v1_5',
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256',
    }),
    'RSASSA-PKCS1-v1_5 generateKey'
  );
  assert_true(
    SubtleCrypto.supports('sign', { name: 'RSASSA-PKCS1-v1_5' }),
    'RSASSA-PKCS1-v1_5 sign'
  );
  assert_true(
    SubtleCrypto.supports('verify', { name: 'RSASSA-PKCS1-v1_5' }),
    'RSASSA-PKCS1-v1_5 verify'
  );

  // ECDSA
  assert_true(
    SubtleCrypto.supports('generateKey', {
      name: 'ECDSA',
      namedCurve: 'P-256',
    }),
    'ECDSA generateKey'
  );
  assert_true(
    SubtleCrypto.supports('sign', { name: 'ECDSA', hash: 'SHA-256' }),
    'ECDSA sign'
  );
  assert_true(
    SubtleCrypto.supports('verify', { name: 'ECDSA', hash: 'SHA-256' }),
    'ECDSA verify'
  );

  // AES-GCM
  assert_true(
    SubtleCrypto.supports('generateKey', { name: 'AES-GCM', length: 256 }),
    'AES-GCM generateKey'
  );
  assert_true(
    SubtleCrypto.supports('encrypt', {
      name: 'AES-GCM',
      iv: new Uint8Array(12),
    }),
    'AES-GCM encrypt'
  );
  assert_true(
    SubtleCrypto.supports('decrypt', {
      name: 'AES-GCM',
      iv: new Uint8Array(12),
    }),
    'AES-GCM decrypt'
  );

  // HMAC
  assert_true(
    SubtleCrypto.supports('generateKey', { name: 'HMAC', hash: 'SHA-256' }),
    'HMAC generateKey'
  );
  assert_true(SubtleCrypto.supports('sign', { name: 'HMAC' }), 'HMAC sign');
  assert_true(SubtleCrypto.supports('verify', { name: 'HMAC' }), 'HMAC verify');
}, 'Common algorithm and operation combinations work');

// Test some specific combinations that should not work
test(() => {
  // Hash algorithms don't support key operations
  assert_false(
    SubtleCrypto.supports('generateKey', 'SHA-256'),
    'SHA-256 generateKey should fail'
  );
  assert_false(
    SubtleCrypto.supports('sign', 'SHA-256'),
    'SHA-256 sign should fail'
  );

  // AES can't sign/verify (these require algorithm parameters due to normalization)
  assert_false(
    SubtleCrypto.supports('sign', 'AES-GCM'),
    'AES-GCM sign should fail'
  );
  assert_false(
    SubtleCrypto.supports('verify', 'AES-GCM'),
    'AES-GCM verify should fail'
  );

  // ECDSA can't encrypt/decrypt
  assert_false(
    SubtleCrypto.supports('encrypt', 'ECDSA'),
    'ECDSA encrypt should fail'
  );
  assert_false(
    SubtleCrypto.supports('decrypt', 'ECDSA'),
    'ECDSA decrypt should fail'
  );

  // HMAC can't encrypt/decrypt
  assert_false(
    SubtleCrypto.supports('encrypt', 'HMAC'),
    'HMAC encrypt should fail'
  );
  assert_false(
    SubtleCrypto.supports('decrypt', 'HMAC'),
    'HMAC decrypt should fail'
  );

  // Non-hash algorithms can't digest
  assert_false(
    SubtleCrypto.supports('digest', 'AES-GCM'),
    'AES-GCM digest should fail'
  );
  assert_false(
    SubtleCrypto.supports('digest', 'ECDSA'),
    'ECDSA digest should fail'
  );
  assert_false(
    SubtleCrypto.supports('digest', 'HMAC'),
    'HMAC digest should fail'
  );
}, 'Invalid algorithm and operation combinations fail');

done();
