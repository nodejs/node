// META: title=WebCryptoAPI: getPublicKey() method
// META: timeout=long
// META: script=util/helpers.js

"use strict";

const algorithms = [
    {
        name: "ECDH",
        generateKeyParams: { name: "ECDH", namedCurve: "P-256" },
        usages: ["deriveKey", "deriveBits"],
        publicKeyUsages: []
    },
    {
        name: "ECDSA",
        generateKeyParams: { name: "ECDSA", namedCurve: "P-256" },
        usages: ["sign", "verify"],
        publicKeyUsages: ["verify"]
    },
    {
        name: "Ed25519",
        generateKeyParams: { name: "Ed25519" },
        usages: ["sign", "verify"],
        publicKeyUsages: ["verify"]
    },
    {
        name: "RSA-OAEP",
        generateKeyParams: {
            name: "RSA-OAEP",
            modulusLength: 2048,
            publicExponent: new Uint8Array([1, 0, 1]),
            hash: "SHA-256"
        },
        usages: ["encrypt", "decrypt"],
        publicKeyUsages: ["encrypt"]
    },
    {
        name: "RSA-PSS",
        generateKeyParams: {
            name: "RSA-PSS",
            modulusLength: 2048,
            publicExponent: new Uint8Array([1, 0, 1]),
            hash: "SHA-256"
        },
        usages: ["sign", "verify"],
        publicKeyUsages: ["verify"]
    },
    {
        name: "RSASSA-PKCS1-v1_5",
        generateKeyParams: {
            name: "RSASSA-PKCS1-v1_5",
            modulusLength: 2048,
            publicExponent: new Uint8Array([1, 0, 1]),
            hash: "SHA-256"
        },
        usages: ["sign", "verify"],
        publicKeyUsages: ["verify"]
    },
    {
        name: "X25519",
        generateKeyParams: { name: "X25519" },
        usages: ["deriveKey", "deriveBits"],
        publicKeyUsages: []
    }
];

// Test basic functionality for supported algorithms
algorithms.forEach(function(algorithm) {
    promise_test(async function(t) {
        // Generate a key pair
        const keyPair = await crypto.subtle.generateKey(
            algorithm.generateKeyParams,
            false, // extractable
            algorithm.usages
        );

        assert_true(keyPair.privateKey instanceof CryptoKey, "Generated private key");
        assert_equals(keyPair.privateKey.type, "private", "Private key type");

        // Test getPublicKey with valid usages
        const publicKey = await crypto.subtle.getPublicKey(
            keyPair.privateKey,
            algorithm.publicKeyUsages
        );

        // Verify the returned public key
        assert_true(publicKey instanceof CryptoKey, "getPublicKey returns a CryptoKey");
        assert_equals(publicKey.type, "public", "Returned key is public");
        assert_equals(publicKey.algorithm.name, algorithm.name, "Algorithm name matches");
        assert_true(publicKey.extractable, "Public key is extractable");

        // Verify usages
        assert_equals(publicKey.usages.length, algorithm.publicKeyUsages.length, "Usage count matches");
        algorithm.publicKeyUsages.forEach(function(usage) {
            assert_true(publicKey.usages.includes(usage), `Has ${usage} usage`);
        });

        // Verify that the derived public key matches the original public key
        // by comparing their exported forms
        const originalExported = await crypto.subtle.exportKey("spki", keyPair.publicKey);
        const derivedExported = await crypto.subtle.exportKey("spki", publicKey);

        assert_array_equals(
            new Uint8Array(originalExported),
            new Uint8Array(derivedExported),
            "Exported public keys match"
        );

    }, `getPublicKey works for ${algorithm.name}`);
});

// Test functional equivalence - ensure derived public key works for crypto operations
promise_test(async function(t) {
    const keyPair = await crypto.subtle.generateKey(
        { name: "ECDSA", namedCurve: "P-256" },
        false,
        ["sign", "verify"]
    );

    const derivedPublicKey = await crypto.subtle.getPublicKey(
        keyPair.privateKey,
        ["verify"]
    );

    // Create test data
    const data = new TextEncoder().encode("test message");

    // Sign with private key
    const signature = await crypto.subtle.sign(
        { name: "ECDSA", hash: "SHA-256" },
        keyPair.privateKey,
        data
    );

    // Verify with both original and derived public keys
    const verifyOriginal = await crypto.subtle.verify(
        { name: "ECDSA", hash: "SHA-256" },
        keyPair.publicKey,
        signature,
        data
    );

    const verifyDerived = await crypto.subtle.verify(
        { name: "ECDSA", hash: "SHA-256" },
        derivedPublicKey,
        signature,
        data
    );

    assert_true(verifyOriginal, "Original public key verifies signature");
    assert_true(verifyDerived, "Derived public key verifies signature");

}, "Derived public key is functionally equivalent to original public key");

// Test with empty usages array
algorithms.forEach(function(algorithm) {
    promise_test(async function(t) {
        // Skip X25519 if not supported
        if (algorithm.name === "X25519") {
            try {
                await crypto.subtle.generateKey(algorithm.generateKeyParams, false, algorithm.usages);
            } catch (e) {
                if (e.name === "NotSupportedError") {
                    return;
                }
                throw e;
            }
        }

        const keyPair = await crypto.subtle.generateKey(
            algorithm.generateKeyParams,
            false,
            algorithm.usages
        );

        // Test with empty usages array
        const publicKey = await crypto.subtle.getPublicKey(
            keyPair.privateKey,
            []
        );

        assert_true(publicKey instanceof CryptoKey, "getPublicKey returns a CryptoKey");
        assert_equals(publicKey.type, "public", "Returned key is public");
        assert_equals(publicKey.usages.length, 0, "Public key has no usages");

    }, `getPublicKey works with empty usages for ${algorithm.name}`);
});

// Test error cases

// Test with non-private key (should throw InvalidAccessError)
promise_test(async function(t) {
    const keyPair = await crypto.subtle.generateKey(
        { name: "ECDSA", namedCurve: "P-256" },
        false,
        ["sign", "verify"]
    );

    await promise_rejects_dom(t, "InvalidAccessError",
        crypto.subtle.getPublicKey(keyPair.publicKey, ["verify"]),
        "getPublicKey should reject when called with a public key"
    );
}, "getPublicKey rejects with InvalidAccessError when given a public key");

// Test with symmetric keys (should throw NotSupportedError for non-private keys)
promise_test(async function(t) {
    const aesKey = await crypto.subtle.generateKey(
        { name: "AES-GCM", length: 256 },
        false,
        ["encrypt", "decrypt"]
    );

    await promise_rejects_dom(t, "NotSupportedError",
        crypto.subtle.getPublicKey(aesKey, []),
        "getPublicKey should reject AES-GCM keys"
    );
}, "getPublicKey rejects with NotSupportedError for AES-GCM symmetric keys");

promise_test(async function(t) {
    const hmacKey = await crypto.subtle.generateKey(
        { name: "HMAC", hash: "SHA-256" },
        false,
        ["sign", "verify"]
    );

    await promise_rejects_dom(t, "NotSupportedError",
        crypto.subtle.getPublicKey(hmacKey, []),
        "getPublicKey should reject HMAC keys"
    );
}, "getPublicKey rejects with NotSupportedError for HMAC symmetric keys");

// Test with invalid usages for the algorithm
promise_test(async function(t) {
    const keyPair = await crypto.subtle.generateKey(
        { name: "ECDSA", namedCurve: "P-256" },
        false,
        ["sign", "verify"]
    );

    // Try to use "encrypt" usage with ECDSA (not supported for ECDSA public keys)
    await promise_rejects_dom(t, "SyntaxError",
        crypto.subtle.getPublicKey(keyPair.privateKey, ["encrypt"]),
        "getPublicKey should reject invalid usages for the algorithm"
    );
}, "getPublicKey rejects with SyntaxError for invalid usages");

// Test with mixed valid and invalid usages
promise_test(async function(t) {
    const keyPair = await crypto.subtle.generateKey(
        { name: "ECDSA", namedCurve: "P-256" },
        false,
        ["sign", "verify"]
    );

    // Mix valid ("verify") and invalid ("encrypt") usages
    await promise_rejects_dom(t, "SyntaxError",
        crypto.subtle.getPublicKey(keyPair.privateKey, ["verify", "encrypt"]),
        "getPublicKey should reject when any usage is invalid"
    );
}, "getPublicKey rejects with SyntaxError when any usage is invalid for the algorithm");

// Test that the method exists
test(function() {
    assert_true("getPublicKey" in crypto.subtle, "getPublicKey method exists on SubtleCrypto");
    assert_equals(typeof crypto.subtle.getPublicKey, "function", "getPublicKey is a function");
}, "getPublicKey method is available");
