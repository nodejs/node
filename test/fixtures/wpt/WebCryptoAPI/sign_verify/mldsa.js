function run_test() {
  runSignatureTests({
    vectors: getTestVectors(),
    invalidVectors: getInvalidTestVectors(),
    algorithmIdentifier(vector) {
      return vector.algorithmName;
    },
    dataLabel: 'plaintext',
  });
}
