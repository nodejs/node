// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js

run_test([
  {
    name: 'ML-DSA-44',
    resultType: 'CryptoKeyPair',
    usages: ['sign', 'verify'],
    publicFormat: 'raw-public',
    privateFormat: 'raw-seed'
  },
  {
    name: 'ML-DSA-65',
    resultType: 'CryptoKeyPair',
    usages: ['sign', 'verify'],
    publicFormat: 'raw-public',
    privateFormat: 'raw-seed'
  },
  {
    name: 'ML-DSA-87',
    resultType: 'CryptoKeyPair',
    usages: ['sign', 'verify'],
    publicFormat: 'raw-public',
    privateFormat: 'raw-seed'
  },
]);
