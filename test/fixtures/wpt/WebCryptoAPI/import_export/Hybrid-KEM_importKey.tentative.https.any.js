// META: title=WebCryptoAPI: importKey() for Hybrid KEM keys
// META: timeout=long
// META: script=../util/helpers.js
// META: script=Hybrid-KEM_importKey_fixtures.js
// META: script=ml_importKey.js

var keyData = hybridKemKeyData;

runTests("MLKEM768-P256");
runTests("MLKEM768-X25519");
runTests("MLKEM1024-P384");
