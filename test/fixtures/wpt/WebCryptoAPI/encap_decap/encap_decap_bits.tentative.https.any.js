// META: title=WebCryptoAPI: ML-KEM encapsulateBits() and decapsulateBits() tests
// META: script=ml_kem_vectors.js
// META: script=../util/helpers.js
// META: timeout=long

function define_bits_tests() {
  var subtle = self.crypto.subtle;
  var variants = ['ML-KEM-512', 'ML-KEM-768', 'ML-KEM-1024'];

  variants.forEach(function (algorithmName) {
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

    // Test round-trip compatibility
    promise_test(async function (test) {
      var keyPair = await subtle.generateKey({ name: algorithmName }, false, [
        'encapsulateBits',
        'decapsulateBits',
      ]);

      var encapsulatedBits = await subtle.encapsulateBits(
        { name: algorithmName },
        keyPair.publicKey
      );

      var decapsulatedBits = await subtle.decapsulateBits(
        { name: algorithmName },
        keyPair.privateKey,
        encapsulatedBits.ciphertext
      );

      assert_true(
        equalBuffers(encapsulatedBits.sharedKey, decapsulatedBits),
        'Encapsulated and decapsulated shared secrets should match'
      );
    }, algorithmName +
      ' encapsulateBits/decapsulateBits round-trip compatibility');

    // Test vector-based decapsulation
    promise_test(async function (test) {
      var vectors = ml_kem_vectors[algorithmName];

      // Import the private key from the vector's privateSeed
      var privateKey = await subtle.importKey(
        'raw-seed',
        vectors.privateSeed,
        { name: algorithmName },
        false,
        ['decapsulateBits']
      );

      // Decapsulate the sample ciphertext from the vectors
      var decapsulatedBits = await subtle.decapsulateBits(
        { name: algorithmName },
        privateKey,
        vectors.sampleCiphertext
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

      // The decapsulated shared secret should match the expected value from vectors
      assert_true(
        equalBuffers(decapsulatedBits, vectors.expectedSharedSecret),
        "Decapsulated shared secret should match vector's expectedSharedSecret"
      );
    }, algorithmName + ' vector-based sampleCiphertext decapsulation');
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

define_bits_tests();
