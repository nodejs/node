// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'HMAC',
    resultType: 'CryptoKey',
    usages: ['sign', 'verify'],
    exportFormat: 'raw'
  },
]);
