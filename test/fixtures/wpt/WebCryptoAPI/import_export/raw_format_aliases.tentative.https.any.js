// META: title=WebCryptoAPI: raw-secret and raw-public importKey() format aliases
// META: timeout=long
// META: script=../util/helpers.js

// For all existing symmetric algorithms in WebCrypto, "raw-secret" acts as an
// alias of "raw". For all existing asymmetric algorithms in WebCrypto,
// "raw-public" acts as an alias of "raw".

"use strict";

const rawKeyData16 = crypto.getRandomValues(new Uint8Array(16));
const rawKeyData32 = crypto.getRandomValues(new Uint8Array(32));
const wrapAlgorithm = { name: "AES-GCM", iv: new Uint8Array(12) };

const symmetricAlgorithms = [
    { algorithm: { name: "AES-CTR", length: 128 }, keyData: rawKeyData16, usages: ["encrypt", "decrypt"] },
    { algorithm: { name: "AES-CBC", length: 128 }, keyData: rawKeyData16, usages: ["encrypt", "decrypt"] },
    { algorithm: { name: "AES-GCM", length: 128 }, keyData: rawKeyData16, usages: ["encrypt", "decrypt"] },
    { algorithm: { name: "AES-KW", length: 128 }, keyData: rawKeyData16, usages: ["wrapKey", "unwrapKey"] },
    { algorithm: { name: "HMAC", hash: "SHA-256", length: 256 }, keyData: rawKeyData32, usages: ["sign", "verify"] },
    { algorithm: { name: "HKDF" }, keyData: rawKeyData32, usages: ["deriveBits", "deriveKey"], extractable: false },
    { algorithm: { name: "PBKDF2" }, keyData: rawKeyData32, usages: ["deriveBits", "deriveKey"], extractable: false },
];

for (const { algorithm, keyData, usages, extractable = true } of symmetricAlgorithms) {
    promise_test(async () => {
        const key = await crypto.subtle.importKey("raw-secret", keyData, algorithm, extractable, usages);
        assert_goodCryptoKey(key, algorithm, extractable, usages, "secret");
        if (extractable) {
            await crypto.subtle.exportKey("raw-secret", key);
        }
    }, `importKey/exportKey with raw-secret: ${algorithm.name}`);

    if (extractable) {
        promise_test(async () => {
            const wrappingKey = await crypto.subtle.generateKey({ name: "AES-GCM", length: 256 }, false, ["wrapKey", "unwrapKey"]);
            const key = await crypto.subtle.importKey("raw-secret", keyData, algorithm, true, usages);
            const wrapped = await crypto.subtle.wrapKey("raw-secret", key, wrappingKey, wrapAlgorithm);
            const unwrapped = await crypto.subtle.unwrapKey("raw-secret", wrapped, wrappingKey, wrapAlgorithm, algorithm, true, usages);
            assert_goodCryptoKey(unwrapped, algorithm, true, usages, "secret");
        }, `wrapKey/unwrapKey with raw-secret: ${algorithm.name}`);
    }
}

const asymmetricAlgorithms = [
    { algorithm: { name: "ECDSA", namedCurve: "P-256" }, usages: ["verify"] },
    { algorithm: { name: "ECDH", namedCurve: "P-256" }, usages: [] },
    { algorithm: { name: "Ed25519" }, usages: ["verify"] },
    { algorithm: { name: "X25519" }, usages: [] },
];

for (const { algorithm, usages } of asymmetricAlgorithms) {
    const generateKeyUsages = usages.length ? usages.concat("sign") : ["deriveBits"];

    promise_test(async () => {
        const keyPair = await crypto.subtle.generateKey(algorithm, true, generateKeyUsages);
        const keyData = await crypto.subtle.exportKey("raw-public", keyPair.publicKey);

        const key = await crypto.subtle.importKey("raw-public", keyData, algorithm, true, usages);
        assert_goodCryptoKey(key, algorithm, true, usages, "public");
        await crypto.subtle.exportKey("raw-public", key);
    }, `importKey/exportKey with raw-public: ${algorithm.name}`);

    promise_test(async () => {
        const keyPair = await crypto.subtle.generateKey(algorithm, true, generateKeyUsages);

        const wrappingKey = await crypto.subtle.generateKey({ name: "AES-GCM", length: 256 }, false, ["wrapKey", "unwrapKey"]);
        const wrapped = await crypto.subtle.wrapKey("raw-public", keyPair.publicKey, wrappingKey, wrapAlgorithm);
        const unwrapped = await crypto.subtle.unwrapKey("raw-public", wrapped, wrappingKey, wrapAlgorithm, algorithm, true, usages);
        assert_goodCryptoKey(unwrapped, algorithm, true, usages, "public");
    }, `wrapKey/unwrapKey with raw-public: ${algorithm.name}`);
}
