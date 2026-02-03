// META: title=WebCryptoAPI: importKey() for symmetric keys
// META: timeout=long
// META: script=../util/helpers.js
// META: script=symmetric_importKey.js

runTests("AES-CTR");
runTests("AES-CBC");
runTests("AES-GCM");
runTests("AES-KW");
runTests("HMAC");
runTests("HKDF");
runTests("PBKDF2");
