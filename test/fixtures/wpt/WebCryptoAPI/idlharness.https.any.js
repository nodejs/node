// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: timeout=long

// https://w3c.github.io/webcrypto/Overview.html

idl_test(
  ['WebCryptoAPI'],
  ['html', 'dom'],
  idl_array => {
    idl_array.add_objects({
      Crypto: ['crypto'],
      SubtleCrypto: ['crypto.subtle']
    });
  }
);
