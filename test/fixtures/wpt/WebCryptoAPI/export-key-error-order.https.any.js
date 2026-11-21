// META: title=WebCryptoAPI: exportKey() and wrapKey() check export support first

// HKDF and PBKDF2 are not registered for the export key operation, and their
// keys can only be imported as non-extractable. Both error conditions are
// therefore always true at once, which makes the order observable.
//
// exportKey: step 6 (NotSupportedError) precedes step 7 (InvalidAccessError).
// https://w3c.github.io/webcrypto/#SubtleCrypto-method-exportKey
//
// wrapKey: step 11 (NotSupportedError) precedes step 12 (InvalidAccessError).
// https://w3c.github.io/webcrypto/#SubtleCrypto-method-wrapKey

function importDeriveKey(name) {
  return crypto.subtle.importKey('raw', new Uint8Array([]), name, false,
                                 ['deriveBits']);
}

function importAesKey(name, usages) {
  return crypto.subtle.importKey('raw', new Uint8Array(16), name, false,
                                 usages);
}

for (const name of ['HKDF', 'PBKDF2']) {
  promise_test(async (t) => {
    const key = await importDeriveKey(name);
    return promise_rejects_dom(t, 'NotSupportedError',
                               crypto.subtle.exportKey('raw', key));
  }, `exportKey() with a ${name} key throws NotSupportedError`);

  promise_test(async (t) => {
    const key = await importDeriveKey(name);
    const wrappingKey = await importAesKey('AES-KW', ['wrapKey']);
    return promise_rejects_dom(
        t, 'NotSupportedError',
        crypto.subtle.wrapKey('raw', key, wrappingKey, 'AES-KW'));
  }, `wrapKey() with a ${name} key throws NotSupportedError`);
}

// An algorithm that does support the export key operation must still report
// the extractable check.
promise_test(async (t) => {
  const key = await importAesKey('AES-CBC', ['encrypt']);
  return promise_rejects_dom(t, 'InvalidAccessError',
                             crypto.subtle.exportKey('raw', key));
}, 'exportKey() with a non-extractable AES-CBC key throws InvalidAccessError');

promise_test(async (t) => {
  const key = await importAesKey('AES-CBC', ['encrypt']);
  const wrappingKey = await importAesKey('AES-KW', ['wrapKey']);
  return promise_rejects_dom(
      t, 'InvalidAccessError',
      crypto.subtle.wrapKey('raw', key, wrappingKey, 'AES-KW'));
}, 'wrapKey() with a non-extractable AES-CBC key throws InvalidAccessError');
