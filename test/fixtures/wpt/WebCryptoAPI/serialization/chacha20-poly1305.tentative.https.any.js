// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'ChaCha20-Poly1305',
    resultType: 'CryptoKey',
    usages: ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'],
    exportFormat: 'raw-secret'
  },
]);
