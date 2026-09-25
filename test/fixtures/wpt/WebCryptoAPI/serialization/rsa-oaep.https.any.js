// META: title=WebCryptoAPI: CryptoKey serialization
// META: script=../util/helpers.js
// META: script=serialization.js
run_test([
  {
    name: 'RSA-OAEP',
    resultType: 'CryptoKeyPair',
    usages: ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'],
    publicFormat: 'spki',
    privateFormat: 'pkcs8'
  },
]);
