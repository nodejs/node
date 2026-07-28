// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'ML-KEM-512',
    resultType: 'CryptoKeyPair',
    usages: [
      'decapsulateBits', 'decapsulateKey', 'encapsulateBits', 'encapsulateKey'
    ],
    publicFormat: 'raw-public',
    privateFormat: 'raw-seed'
  },
  {
    name: 'ML-KEM-768',
    resultType: 'CryptoKeyPair',
    usages: [
      'decapsulateBits', 'decapsulateKey', 'encapsulateBits', 'encapsulateKey'
    ],
    publicFormat: 'raw-public',
    privateFormat: 'raw-seed'
  },
  {
    name: 'ML-KEM-1024',
    resultType: 'CryptoKeyPair',
    usages: [
      'decapsulateBits', 'decapsulateKey', 'encapsulateBits', 'encapsulateKey'
    ],
    publicFormat: 'raw-public',
    privateFormat: 'raw-seed'
  },
]);
