// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'AES-CTR',
    resultType: 'CryptoKey',
    usages: ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'],
    exportFormat: 'raw'
  },
]);
