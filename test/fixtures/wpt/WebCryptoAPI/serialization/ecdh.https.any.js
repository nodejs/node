// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'ECDH',
    resultType: 'CryptoKeyPair',
    usages: ['deriveKey', 'deriveBits'],
    publicFormat: 'raw',
    privateFormat: 'pkcs8'
  },
]);
