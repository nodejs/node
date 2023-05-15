// META: title=WebCryptoAPI: deriveBits() and deriveKey() Using PBKDF2
// META: timeout=long
// META: variant=?1-500
// META: variant=?501-1000
// META: variant=?1001-1500
// META: variant=?1501-2000
// META: variant=?2001-2500
// META: variant=?2501-3000
// META: variant=?3001-3500
// META: variant=?3501-4000
// META: variant=?4001-4500
// META: variant=?4501-5000
// META: variant=?5001-5500
// META: variant=?5501-6000
// META: variant=?6001-6500
// META: variant=?6501-7000
// META: variant=?7001-7500
// META: variant=?7501-8000
// META: variant=?8001-8500
// META: variant=?8501-last
// META: script=/common/subset-tests.js
// META: script=pbkdf2_vectors.js
// META: script=pbkdf2.js

// Define subtests from a `promise_test` to ensure the harness does not
// complete before the subtests are available. `explicit_done` cannot be used
// for this purpose because the global `done` function is automatically invoked
// by the WPT infrastructure in dedicated worker tests defined using the
// "multi-global" pattern.
promise_test(define_tests, 'setup - define tests');
