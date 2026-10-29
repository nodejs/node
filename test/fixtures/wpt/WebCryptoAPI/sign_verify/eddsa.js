function run_test(algorithmName) {
  runSignatureTests({
    vectors: getTestVectors(algorithmName),
    algorithmIdentifier(vector) {
      return {name: vector.algorithmName};
    },
    katFirst: true,
    generatedKeys: true,
  });
}
