// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'RSASSA-PKCS1-v1_5',
    resultType: 'CryptoKeyPair',
    usages: ['sign', 'verify'],
    publicFormat: 'spki',
    privateFormat: 'pkcs8'
  },
]);
