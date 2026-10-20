// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: timeout=long

// https://wicg.github.io/webcrypto-modern-algos/

idl_test(
  ['webcrypto-modern-algos'],
  ['webcrypto', 'html', 'dom'],
  idl_array => {
    idl_array.add_objects({
      Crypto: ['crypto'],
      SubtleCrypto: ['crypto.subtle']
    });
  }
);
