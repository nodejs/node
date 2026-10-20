// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'AES-KW',
    resultType: 'CryptoKey',
    usages: ['wrapKey', 'unwrapKey'],
    exportFormat: 'raw'
  },
]);
