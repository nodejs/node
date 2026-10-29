// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'RSA-PSS',
    resultType: 'CryptoKeyPair',
    usages: ['sign', 'verify'],
    publicFormat: 'spki',
    privateFormat: 'pkcs8'
  },
]);
