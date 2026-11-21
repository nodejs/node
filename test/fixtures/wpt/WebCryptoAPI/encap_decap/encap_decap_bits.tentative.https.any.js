// META: title=WebCryptoAPI: KEM encapsulateBits() and decapsulateBits() tests
// META: script=../util/helpers.js
// META: script=ml_kem_vectors.js
// META: script=hybrid_kem_vectors.js
// META: timeout=long

function define_bits_tests() {
  var subtle = self.crypto.subtle;
  var variants = [
    { name: 'ML-KEM-512', ciphertextLength: 768 },
    { name: 'ML-KEM-768', ciphertextLength: 1088 },
    { name: 'ML-KEM-1024', ciphertextLength: 1568 },
    { name: 'MLKEM768-P256', ciphertextLength: 1153 },
    { name: 'MLKEM768-X25519', ciphertextLength: 1120 },
    { name: 'MLKEM1024-P384', ciphertextLength: 1665 },
  ];

  variants.forEach(function (variant) {
    var algorithmName = variant.name;

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
        Object.hasOwn(encapsulatedBits, 'sharedKey'),
        'Result should have sharedKey property'
      );
      assert_true(
        Object.hasOwn(encapsulatedBits, 'ciphertext'),
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

      assert_equals(
        encapsulatedBits.ciphertext.byteLength,
        variant.ciphertextLength,
        'Ciphertext should be ' +
          variant.ciphertextLength +
          ' bytes for ' +
          algorithmName
      );
    }, algorithmName + ' encapsulateBits basic functionality');

    // Test encapsulateBits/decapsulateBits round-trip compatibility
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

define_bits_tests();
