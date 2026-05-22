// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'KMAC128',
    resultType: 'CryptoKey',
    usages: ['sign', 'verify'],
    exportFormat: 'raw-secret'
  },
  {
    name: 'KMAC256',
    resultType: 'CryptoKey',
    usages: ['sign', 'verify'],
    exportFormat: 'raw-secret'
  },
]);
