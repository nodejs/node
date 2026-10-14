// META: title=WebCryptoAPI: generateKey() Successful Calls
// META: timeout=long
// META: script=../../util/helpers.js
// META: script=/common/subset-tests.js
// META: script=../../generateKey/algorithm_registry.js
// META: script=../../generateKey/successes.js
run_test(["MLKEM768-P256", "MLKEM768-X25519", "MLKEM1024-P384"]);
