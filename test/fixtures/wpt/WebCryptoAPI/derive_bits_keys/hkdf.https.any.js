// META: title=WebCryptoAPI: deriveBits() and deriveKey() Using HKDF
// META: variant=?1-1000
// META: variant=?1001-2000
// META: variant=?2001-3000
// META: variant=?3001-last
// META: script=/common/subset-tests.js
// META: script=hkdf_vectors.js
// META: script=hkdf.js

// Define subtests from a `promise_test` to ensure the harness does not
// complete before the subtests are available. `explicit_done` cannot be used
// for this purpose because the global `done` function is automatically invoked
// by the WPT infrastructure in dedicated worker tests defined using the
// "multi-global" pattern.
promise_test(define_tests, 'setup - define tests');
