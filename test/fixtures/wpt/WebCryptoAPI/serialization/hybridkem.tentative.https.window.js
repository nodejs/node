// META: title=WebCryptoAPI: Hybrid KEM CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'MLKEM768-P256',
    resultType: 'CryptoKeyPair',
    usages: [
      'decapsulateBits', 'decapsulateKey', 'encapsulateBits', 'encapsulateKey'
    ],
    publicFormat: 'raw-public',
    privateFormat: 'raw-seed'
  },
  {
    name: 'MLKEM768-X25519',
    resultType: 'CryptoKeyPair',
    usages: [
      'decapsulateBits', 'decapsulateKey', 'encapsulateBits', 'encapsulateKey'
    ],
    publicFormat: 'raw-public',
    privateFormat: 'raw-seed'
  },
  {
    name: 'MLKEM1024-P384',
    resultType: 'CryptoKeyPair',
    usages: [
      'decapsulateBits', 'decapsulateKey', 'encapsulateBits', 'encapsulateKey'
    ],
    publicFormat: 'raw-public',
    privateFormat: 'raw-seed'
  },
]);
