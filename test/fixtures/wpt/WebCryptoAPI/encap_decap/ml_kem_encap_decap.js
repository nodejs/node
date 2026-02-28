// Test implementation for ML-KEM encapsulate and decapsulate operations

function define_tests() {
  var subtle = self.crypto.subtle;

  // Test data for all ML-KEM variants
  var variants = ['ML-KEM-512', 'ML-KEM-768', 'ML-KEM-1024'];

  variants.forEach(function (algorithmName) {
    var testVector = ml_kem_vectors[algorithmName];

    // Test encapsulateBits operation
    promise_test(async function (test) {
      // Generate a key pair for testing
      var keyPair = await subtle.generateKey({ name: algorithmName }, false, [
        'encapsulateBits',
        'decapsulateBits',
      ]);

      // Test encapsulateBits
      var encapsulatedBits = await subtle.encapsulateBits(
        { name: algorithmName },
        keyPair.publicKey
      );

      assert_true(
        encapsulatedBits instanceof Object,
        'encapsulateBits should return an object'
      );
      assert_true(
        encapsulatedBits.hasOwnProperty('sharedKey'),
        'Result should have sharedKey property'
      );
      assert_true(
        encapsulatedBits.hasOwnProperty('ciphertext'),
        'Result should have ciphertext property'
      );
      assert_true(
        encapsulatedBits.sharedKey instanceof ArrayBuffer,
        'sharedKey should be ArrayBuffer'
      );
      assert_true(
        encapsulatedBits.ciphertext instanceof ArrayBuffer,
        'ciphertext should be ArrayBuffer'
      );

      // Verify sharedKey length (should be 32 bytes for all ML-KEM variants)
      assert_equals(
        encapsulatedBits.sharedKey.byteLength,
        32,
        'Shared key should be 32 bytes'
      );

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
        encapsulatedBits.ciphertext.byteLength,
        expectedCiphertextLength,
        'Ciphertext should be ' +
          expectedCiphertextLength +
          ' bytes for ' +
          algorithmName
      );
    }, algorithmName + ' encapsulateBits basic functionality');

    // Test decapsulateBits operation
    promise_test(async function (test) {
      // Generate a key pair for testing
      var keyPair = await subtle.generateKey({ name: algorithmName }, false, [
        'encapsulateBits',
        'decapsulateBits',
      ]);

      // First encapsulate to get ciphertext
      var encapsulatedBits = await subtle.encapsulateBits(
        { name: algorithmName },
        keyPair.publicKey
      );

      // Then decapsulate using the private key
      var decapsulatedBits = await subtle.decapsulateBits(
        { name: algorithmName },
        keyPair.privateKey,
        encapsulatedBits.ciphertext
      );

      assert_true(
        decapsulatedBits instanceof ArrayBuffer,
        'decapsulateBits should return ArrayBuffer'
      );
      assert_equals(
        decapsulatedBits.byteLength,
        32,
        'Decapsulated bits should be 32 bytes'
      );

      // The decapsulated shared secret should match the original
      assert_true(
        equalBuffers(decapsulatedBits, encapsulatedBits.sharedKey),
        'Decapsulated shared secret should match original'
      );
    }, algorithmName + ' decapsulateBits basic functionality');

    // Test encapsulateKey operation
    promise_test(async function (test) {
      // Generate a key pair for testing
      var keyPair = await subtle.generateKey({ name: algorithmName }, false, [
        'encapsulateKey',
        'decapsulateKey',
      ]);

      // Test encapsulateKey with AES-GCM as the shared key algorithm
      var encapsulatedKey = await subtle.encapsulateKey(
        { name: algorithmName },
        keyPair.publicKey,
        { name: 'AES-GCM', length: 256 },
        true,
        ['encrypt', 'decrypt']
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
        'AES-GCM',
        'Shared key algorithm should be AES-GCM'
      );
      assert_equals(
        encapsulatedKey.sharedKey.algorithm.length,
        256,
        'Shared key length should be 256'
      );
      assert_true(
        encapsulatedKey.sharedKey.extractable,
        'Shared key should be extractable as specified'
      );
      assert_array_equals(
        encapsulatedKey.sharedKey.usages,
        ['encrypt', 'decrypt'],
        'Shared key should have correct usages'
      );
    }, algorithmName + ' encapsulateKey basic functionality');

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
        { name: 'AES-GCM', length: 256 },
        true,
        ['encrypt', 'decrypt']
      );

      // Then decapsulate using the private key
      var decapsulatedKey = await subtle.decapsulateKey(
        { name: algorithmName },
        keyPair.privateKey,
        encapsulatedKey.ciphertext,
        { name: 'AES-GCM', length: 256 },
        true,
        ['encrypt', 'decrypt']
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
        'AES-GCM',
        'Decapsulated key algorithm should be AES-GCM'
      );
      assert_equals(
        decapsulatedKey.algorithm.length,
        256,
        'Decapsulated key length should be 256'
      );
      assert_true(
        decapsulatedKey.extractable,
        'Decapsulated key should be extractable as specified'
      );
      assert_array_equals(
        decapsulatedKey.usages,
        ['encrypt', 'decrypt'],
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
    }, algorithmName + ' decapsulateKey basic functionality');

    // Test error cases for encapsulateBits
    promise_test(async function (test) {
      var keyPair = await subtle.generateKey({ name: algorithmName }, false, [
        'encapsulateBits',
        'decapsulateBits',
      ]);

      // Test with wrong key type (private key instead of public)
      await promise_rejects_dom(
        test,
        'InvalidAccessError',
        subtle.encapsulateBits({ name: algorithmName }, keyPair.privateKey),
        'encapsulateBits should reject private key'
      );

      // Test with wrong algorithm name
      await promise_rejects_dom(
        test,
        'InvalidAccessError',
        subtle.encapsulateBits({ name: 'AES-GCM' }, keyPair.publicKey),
        'encapsulateBits should reject mismatched algorithm'
      );
    }, algorithmName + ' encapsulateBits error cases');

    // Test error cases for decapsulateBits
    promise_test(async function (test) {
      var keyPair = await subtle.generateKey({ name: algorithmName }, false, [
        'encapsulateBits',
        'decapsulateBits',
      ]);

      var encapsulatedBits = await subtle.encapsulateBits(
        { name: algorithmName },
        keyPair.publicKey
      );

      // Test with wrong key type (public key instead of private)
      await promise_rejects_dom(
        test,
        'InvalidAccessError',
        subtle.decapsulateBits(
          { name: algorithmName },
          keyPair.publicKey,
          encapsulatedBits.ciphertext
        ),
        'decapsulateBits should reject public key'
      );

      // Test with wrong algorithm name
      await promise_rejects_dom(
        test,
        'InvalidAccessError',
        subtle.decapsulateBits(
          { name: 'AES-GCM' },
          keyPair.privateKey,
          encapsulatedBits.ciphertext
        ),
        'decapsulateBits should reject mismatched algorithm'
      );

      // Test with invalid ciphertext
      var invalidCiphertext = new Uint8Array(10); // Wrong size
      await promise_rejects_dom(
        test,
        'OperationError',
        subtle.decapsulateBits(
          { name: algorithmName },
          keyPair.privateKey,
          invalidCiphertext
        ),
        'decapsulateBits should reject invalid ciphertext'
      );
    }, algorithmName + ' decapsulateBits error cases');

    // Test error cases for encapsulateKey
    promise_test(async function (test) {
      var keyPair = await subtle.generateKey({ name: algorithmName }, false, [
        'encapsulateKey',
        'decapsulateKey',
      ]);

      // Test with key without encapsulateKey usage
      var wrongKeyPair = await subtle.generateKey(
        { name: algorithmName },
        false,
        ['decapsulateKey'] // Missing encapsulateKey usage
      );

      await promise_rejects_dom(
        test,
        'InvalidAccessError',
        subtle.encapsulateKey(
          { name: algorithmName },
          wrongKeyPair.publicKey,
          { name: 'AES-GCM', length: 256 },
          true,
          ['encrypt', 'decrypt']
        ),
        'encapsulateKey should reject key without encapsulateKey usage'
      );
    }, algorithmName + ' encapsulateKey error cases');

    // Test error cases for decapsulateKey
    promise_test(async function (test) {
      var keyPair = await subtle.generateKey({ name: algorithmName }, false, [
        'encapsulateKey',
        'decapsulateKey',
      ]);

      var encapsulatedKey = await subtle.encapsulateKey(
        { name: algorithmName },
        keyPair.publicKey,
        { name: 'AES-GCM', length: 256 },
        true,
        ['encrypt', 'decrypt']
      );

      // Test with key without decapsulateKey usage
      var wrongKeyPair = await subtle.generateKey(
        { name: algorithmName },
        false,
        ['encapsulateKey'] // Missing decapsulateKey usage
      );

      await promise_rejects_dom(
        test,
        'InvalidAccessError',
        subtle.decapsulateKey(
          { name: algorithmName },
          wrongKeyPair.privateKey,
          encapsulatedKey.ciphertext,
          { name: 'AES-GCM', length: 256 },
          true,
          ['encrypt', 'decrypt']
        ),
        'decapsulateKey should reject key without decapsulateKey usage'
      );
    }, algorithmName + ' decapsulateKey error cases');
  });
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

function run_test() {
  define_tests();
}
