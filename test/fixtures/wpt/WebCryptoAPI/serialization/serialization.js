function run_test(vectors) {
  function testCryptoKeySerialization(
      generateKeyAlgorithm, generateKeyUsages, exportFormat) {
    promise_test(async t => {
      var cryptoKey = await crypto.subtle.generateKey(
          generateKeyAlgorithm, true, generateKeyUsages);
      const keyExported =
          await crypto.subtle.exportKey(exportFormat, cryptoKey);

      const {key} = structuredClone({key: cryptoKey});
      const newKeyExported =
          await crypto.subtle.exportKey(exportFormat, key);
      assert_true(equalBuffers(keyExported, newKeyExported));
    }, 'serialization test ' + objectToString(generateKeyAlgorithm));
  };

  function testCryptoKeyPairSerialization(
      generateKeyAlgorithm, generateKeyUsages, publicExportFormat,
      privateExportFormat) {
    promise_test(async t => {
      var keyPair = await crypto.subtle.generateKey(
          generateKeyAlgorithm, true, generateKeyUsages);
      const publicKeyExported =
          await crypto.subtle.exportKey(publicExportFormat, keyPair.publicKey);
      const privateKeyExported = await crypto.subtle.exportKey(
          privateExportFormat, keyPair.privateKey);

      const {publicKey, privateKey} = structuredClone(
          {publicKey: keyPair.publicKey, privateKey: keyPair.privateKey});
      const newPublicKeyExported =
          await crypto.subtle.exportKey(publicExportFormat, publicKey);
      assert_true(equalBuffers(publicKeyExported, newPublicKeyExported));
      const newPrivateKeyExported = await crypto.subtle.exportKey(
          privateExportFormat, privateKey);
      assert_true(equalBuffers(privateKeyExported, newPrivateKeyExported));
    }, 'serialization test ' + objectToString(generateKeyAlgorithm));
  };

  vectors.forEach(function(vector) {
    if (vector.resultType === 'CryptoKey') {
      allAlgorithmSpecifiersFor(vector.name)
          .forEach(function(generateKeyAlgorithm) {
            testCryptoKeySerialization(
                generateKeyAlgorithm, vector.usages, vector.exportFormat);
          });
    } else {
      allAlgorithmSpecifiersFor(vector.name)
          .forEach(function(generateKeyAlgorithm) {
            testCryptoKeyPairSerialization(
                generateKeyAlgorithm, vector.usages, vector.publicFormat,
                vector.privateFormat);
          });
    }
  });
}
