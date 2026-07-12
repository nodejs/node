// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'Ed25519',
    resultType: 'CryptoKeyPair',
    usages: ['sign', 'verify'],
    publicFormat: 'raw',
    privateFormat: 'pkcs8'
  },
]);
