// META: title=WebCryptoAPI: ML-KEM encapsulateKey() and decapsulateKey() tests
// META: script=ml_kem_vectors.js
// META: script=../util/helpers.js
// META: timeout=long

function define_key_tests() {
  var subtle = self.crypto.subtle;
  var variants = ['ML-KEM-512', 'ML-KEM-768', 'ML-KEM-1024'];

  // Test various 256-bit shared key algorithms
  var sharedKeyConfigs = [
    {
      algorithm: { name: 'AES-GCM', length: 256 },
      usages: ['encrypt', 'decrypt'],
      description: 'AES-GCM-256',
    },
    {
      algorithm: { name: 'AES-CBC', length: 256 },
      usages: ['encrypt', 'decrypt'],
      description: 'AES-CBC-256',
    },
    {
      algorithm: { name: 'AES-CTR', length: 256 },
      usages: ['encrypt', 'decrypt'],
      description: 'AES-CTR-256',
    },
    {
      algorithm: { name: 'AES-KW', length: 256 },
      usages: ['wrapKey', 'unwrapKey'],
      description: 'AES-KW-256',
    },
    {
      algorithm: { name: 'HMAC', hash: 'SHA-256' },
      usages: ['sign', 'verify'],
      description: 'HMAC-SHA-256',
    },
  ];

  variants.forEach(function (algorithmName) {
    sharedKeyConfigs.forEach(function (config) {
      // Test encapsulateKey operation
      promise_test(async function (test) {
        // Generate a key pair for testing
        var keyPair = await subtle.generateKey({ name: algorithmName }, false, [
          'encapsulateKey',
          'decapsulateKey',
        ]);

        // Test encapsulateKey
        var encapsulatedKey = await subtle.encapsulateKey(
          { name: algorithmName },
          keyPair.publicKey,
          config.algorithm,
          true,
          config.usages
        );

        assert_true(
          encapsulatedKey instanceof Object,
          'encapsulateKey should return an object'
        );
        assert_true(
          encapsulatedKey.hasOwnProperty('sharedKey'),
          'Result should have sharedKey property'
        );
        assert_true(
          encapsulatedKey.hasOwnProperty('ciphertext'),
          'Result should have ciphertext property'
        );
        assert_true(
          encapsulatedKey.sharedKey instanceof CryptoKey,
          'sharedKey should be a CryptoKey'
        );
        assert_true(
          encapsulatedKey.ciphertext instanceof ArrayBuffer,
          'ciphertext should be ArrayBuffer'
        );

        // Verify the shared key properties
        assert_equals(
          encapsulatedKey.sharedKey.type,
          'secret',
          'Shared key should be secret type'
        );
        assert_equals(
          encapsulatedKey.sharedKey.algorithm.name,
          config.algorithm.name,
          'Shared key algorithm should match'
        );
        assert_true(
          encapsulatedKey.sharedKey.extractable,
          'Shared key should be extractable as specified'
        );
        assert_array_equals(
          encapsulatedKey.sharedKey.usages,
          config.usages,
          'Shared key should have correct usages'
        );

        // Verify algorithm-specific properties
        if (config.algorithm.length) {
          assert_equals(
            encapsulatedKey.sharedKey.algorithm.length,
            config.algorithm.length,
            'Key length should be 256'
          );
        }
        if (config.algorithm.hash) {
          assert_equals(
            encapsulatedKey.sharedKey.algorithm.hash.name,
            config.algorithm.hash,
            'Hash algorithm should match'
          );
        }

        // Verify ciphertext length based on algorithm variant
        var expectedCiphertextLength;
        switch (algorithmName) {
          case 'ML-KEM-512':
            expectedCiphertextLength = 768;
            break;
          case 'ML-KEM-768':
            expectedCiphertextLength = 1088;
            break;
          case 'ML-KEM-1024':
            expectedCiphertextLength = 1568;
            break;
        }
        assert_equals(
          encapsulatedKey.ciphertext.byteLength,
          expectedCiphertextLength,
          'Ciphertext should be ' +
            expectedCiphertextLength +
            ' bytes for ' +
            algorithmName
        );
      }, algorithmName + ' encapsulateKey with ' + config.description);

      // Test decapsulateKey operation
      promise_test(async function (test) {
        // Generate a key pair for testing
        var keyPair = await subtle.generateKey({ name: algorithmName }, false, [
          'encapsulateKey',
          'decapsulateKey',
        ]);

        // First encapsulate to get ciphertext
        var encapsulatedKey = await subtle.encapsulateKey(
          { name: algorithmName },
          keyPair.publicKey,
          config.algorithm,
          true,
          config.usages
        );

        // Then decapsulate using the private key
        var decapsulatedKey = await subtle.decapsulateKey(
          { name: algorithmName },
          keyPair.privateKey,
          encapsulatedKey.ciphertext,
          config.algorithm,
          true,
          config.usages
        );

        assert_true(
          decapsulatedKey instanceof CryptoKey,
          'decapsulateKey should return a CryptoKey'
        );
        assert_equals(
          decapsulatedKey.type,
          'secret',
          'Decapsulated key should be secret type'
        );
        assert_equals(
          decapsulatedKey.algorithm.name,
          config.algorithm.name,
          'Decapsulated key algorithm should match'
        );
        assert_true(
          decapsulatedKey.extractable,
          'Decapsulated key should be extractable as specified'
        );
        assert_array_equals(
          decapsulatedKey.usages,
          config.usages,
          'Decapsulated key should have correct usages'
        );

        // Extract both keys and verify they are identical
        var originalKeyMaterial = await subtle.exportKey(
          'raw',
          encapsulatedKey.sharedKey
        );
        var decapsulatedKeyMaterial = await subtle.exportKey(
          'raw',
          decapsulatedKey
        );

        assert_true(
          equalBuffers(originalKeyMaterial, decapsulatedKeyMaterial),
          'Decapsulated key material should match original'
        );

        // Verify the key material is 32 bytes (256 bits)
        assert_equals(
          originalKeyMaterial.byteLength,
          32,
          'Shared key material should be 32 bytes'
        );
      }, algorithmName + ' decapsulateKey with ' + config.description);

      // Test round-trip compatibility
      promise_test(async function (test) {
        var keyPair = await subtle.generateKey({ name: algorithmName }, false, [
          'encapsulateKey',
          'decapsulateKey',
        ]);

        var encapsulatedKey = await subtle.encapsulateKey(
          { name: algorithmName },
          keyPair.publicKey,
          config.algorithm,
          true,
          config.usages
        );

        var decapsulatedKey = await subtle.decapsulateKey(
          { name: algorithmName },
          keyPair.privateKey,
          encapsulatedKey.ciphertext,
          config.algorithm,
          true,
          config.usages
        );

        // Verify keys have the same material
        var originalKeyMaterial = await subtle.exportKey(
          'raw',
          encapsulatedKey.sharedKey
        );
        var decapsulatedKeyMaterial = await subtle.exportKey(
          'raw',
          decapsulatedKey
        );

        assert_true(
          equalBuffers(originalKeyMaterial, decapsulatedKeyMaterial),
          'Encapsulated and decapsulated keys should have the same material'
        );

        // Test that the derived keys can actually be used for their intended purpose
        if (
          config.algorithm.name.startsWith('AES') &&
          config.usages.includes('encrypt')
        ) {
          await testAESOperation(
            encapsulatedKey.sharedKey,
            decapsulatedKey,
            config.algorithm
          );
        } else if (config.algorithm.name === 'HMAC') {
          await testHMACOperation(encapsulatedKey.sharedKey, decapsulatedKey);
        }
      }, algorithmName +
        ' encapsulateKey/decapsulateKey round-trip with ' +
        config.description);
    });

    // Test vector-based decapsulation for each shared key config
    sharedKeyConfigs.forEach(function (config) {
      promise_test(async function (test) {
        var vectors = ml_kem_vectors[algorithmName];

        // Import the private key from the vector's privateSeed
        var privateKey = await subtle.importKey(
          'raw-seed',
          vectors.privateSeed,
          { name: algorithmName },
          false,
          ['decapsulateKey']
        );

        // Decapsulate the sample ciphertext from the vectors to get a shared key
        var decapsulatedKey = await subtle.decapsulateKey(
          { name: algorithmName },
          privateKey,
          vectors.sampleCiphertext,
          config.algorithm,
          true,
          config.usages
        );

        assert_true(
          decapsulatedKey instanceof CryptoKey,
          'decapsulateKey should return a CryptoKey'
        );
        assert_equals(
          decapsulatedKey.type,
          'secret',
          'Decapsulated key should be secret type'
        );
        assert_equals(
          decapsulatedKey.algorithm.name,
          config.algorithm.name,
          'Decapsulated key algorithm should match'
        );
        assert_true(
          decapsulatedKey.extractable,
          'Decapsulated key should be extractable as specified'
        );
        assert_array_equals(
          decapsulatedKey.usages,
          config.usages,
          'Decapsulated key should have correct usages'
        );

        // Extract the key material and verify it matches the expected shared secret
        var keyMaterial = await subtle.exportKey('raw', decapsulatedKey);
        assert_equals(
          keyMaterial.byteLength,
          32,
          'Shared key material should be 32 bytes'
        );
        assert_true(
          equalBuffers(keyMaterial, vectors.expectedSharedSecret),
          "Decapsulated key material should match vector's expectedSharedSecret"
        );

        // Verify algorithm-specific properties
        if (config.algorithm.length) {
          assert_equals(
            decapsulatedKey.algorithm.length,
            config.algorithm.length,
            'Key length should be 256'
          );
        }
        if (config.algorithm.hash) {
          assert_equals(
            decapsulatedKey.algorithm.hash.name,
            config.algorithm.hash,
            'Hash algorithm should match'
          );
        }
      }, algorithmName +
        ' vector-based sampleCiphertext decapsulation with ' +
        config.description);
    });
  });
}

async function testAESOperation(key1, key2, algorithm) {
  var plaintext = new Uint8Array([
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
  ]);

  if (algorithm.name === 'AES-GCM') {
    var iv = crypto.getRandomValues(new Uint8Array(12));
    var params = { name: 'AES-GCM', iv: iv };

    var ciphertext1 = await crypto.subtle.encrypt(params, key1, plaintext);
    var ciphertext2 = await crypto.subtle.encrypt(params, key2, plaintext);

    assert_true(
      equalBuffers(ciphertext1, ciphertext2),
      'AES-GCM encryption should produce identical results'
    );

    var decrypted1 = await crypto.subtle.decrypt(params, key1, ciphertext1);
    var decrypted2 = await crypto.subtle.decrypt(params, key2, ciphertext2);

    assert_true(
      equalBuffers(decrypted1, plaintext),
      'AES-GCM decryption should work with key1'
    );
    assert_true(
      equalBuffers(decrypted2, plaintext),
      'AES-GCM decryption should work with key2'
    );
  } else if (algorithm.name === 'AES-CBC') {
    var iv = crypto.getRandomValues(new Uint8Array(16));
    var params = { name: 'AES-CBC', iv: iv };

    var ciphertext1 = await crypto.subtle.encrypt(params, key1, plaintext);
    var ciphertext2 = await crypto.subtle.encrypt(params, key2, plaintext);

    assert_true(
      equalBuffers(ciphertext1, ciphertext2),
      'AES-CBC encryption should produce identical results'
    );
  } else if (algorithm.name === 'AES-CTR') {
    var counter = crypto.getRandomValues(new Uint8Array(16));
    var params = { name: 'AES-CTR', counter: counter, length: 128 };

    var ciphertext1 = await crypto.subtle.encrypt(params, key1, plaintext);
    var ciphertext2 = await crypto.subtle.encrypt(params, key2, plaintext);

    assert_true(
      equalBuffers(ciphertext1, ciphertext2),
      'AES-CTR encryption should produce identical results'
    );
  }
}

async function testHMACOperation(key1, key2) {
  var data = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);

  var signature1 = await crypto.subtle.sign({ name: 'HMAC' }, key1, data);
  var signature2 = await crypto.subtle.sign({ name: 'HMAC' }, key2, data);

  assert_true(
    equalBuffers(signature1, signature2),
    'HMAC signatures should be identical'
  );

  var verified1 = await crypto.subtle.verify(
    { name: 'HMAC' },
    key1,
    signature1,
    data
  );
  var verified2 = await crypto.subtle.verify(
    { name: 'HMAC' },
    key2,
    signature2,
    data
  );

  assert_true(verified1, 'HMAC verification should succeed with key1');
  assert_true(verified2, 'HMAC verification should succeed with key2');
}

// Helper function to compare two ArrayBuffers
function equalBuffers(a, b) {
  if (a.byteLength !== b.byteLength) {
    return false;
  }
  var aBytes = new Uint8Array(a);
  var bBytes = new Uint8Array(b);
  for (var i = 0; i < a.byteLength; i++) {
    if (aBytes[i] !== bBytes[i]) {
      return false;
    }
  }
  return true;
}

define_key_tests();
