// META: title=WebCryptoAPI: importKey() for ML-KEM keys
// META: timeout=long
// META: script=../util/helpers.js
// META: script=ML-KEM_importKey_fixtures.js
// META: script=ML-KEM_importKey.js

runTests("ML-KEM-512");
runTests("ML-KEM-768");
runTests("ML-KEM-1024");
