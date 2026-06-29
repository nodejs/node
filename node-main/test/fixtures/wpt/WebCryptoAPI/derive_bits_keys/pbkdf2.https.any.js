// META: title=WebCryptoAPI: deriveBits() and deriveKey() Using PBKDF2
// META: timeout=long
// META: variant=?1-1000
// META: variant=?1001-2000
// META: variant=?2001-3000
// META: variant=?3001-4000
// META: variant=?4001-5000
// META: variant=?5001-6000
// META: variant=?6001-7000
// META: variant=?7001-8000
// META: variant=?8001-last
// META: script=/common/subset-tests.js
// META: script=pbkdf2_vectors.js
// META: script=pbkdf2.js

// Define subtests from a `promise_test` to ensure the harness does not
// complete before the subtests are available. `explicit_done` cannot be used
// for this purpose because the global `done` function is automatically invoked
// by the WPT infrastructure in dedicated worker tests defined using the
// "multi-global" pattern.
promise_test(define_tests, 'setup - define tests');
