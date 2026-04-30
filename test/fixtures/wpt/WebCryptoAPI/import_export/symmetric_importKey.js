// Test importKey and exportKey for non-PKC algorithms. Only "happy paths" are
// currently tested - those where the operation should succeed.


function runTests(algorithmName) {
    var subtle = crypto.subtle;

    // keying material for algorithms that can use any bit string.
    var rawKeyData = [
        new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]),
        new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                        17, 18, 19, 20, 21, 22, 23, 24]),
        new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32])
    ];

    // combinations of algorithms, usages, parameters, and formats to test
    var testVectors = [
        {name: "AES-CTR",               legalUsages: ["encrypt", "decrypt"],      extractable: [true, false], formats: ["raw", "jwk"]},
        {name: "AES-CBC",               legalUsages: ["encrypt", "decrypt"],      extractable: [true, false], formats: ["raw", "jwk"]},
        {name: "AES-GCM",               legalUsages: ["encrypt", "decrypt"],      extractable: [true, false], formats: ["raw", "jwk"]},
        {name: "AES-KW",                legalUsages: ["wrapKey", "unwrapKey"],    extractable: [true, false], formats: ["raw", "jwk"]},
        {name: "HMAC", hash: "SHA-1",   legalUsages: ["sign", "verify"],          extractable: [true, false], formats: ["raw", "jwk"]},
        {name: "HMAC", hash: "SHA-256", legalUsages: ["sign", "verify"],          extractable: [true, false], formats: ["raw", "jwk"]},
        {name: "HMAC", hash: "SHA-384", legalUsages: ["sign", "verify"],          extractable: [true, false], formats: ["raw", "jwk"]},
        {name: "HMAC", hash: "SHA-512", legalUsages: ["sign", "verify"],          extractable: [true, false], formats: ["raw", "jwk"]},
        {name: "HKDF",                  legalUsages: ["deriveBits", "deriveKey"], extractable: [false],       formats: ["raw"]},
        {name: "PBKDF2",                legalUsages: ["deriveBits", "deriveKey"], extractable: [false],       formats: ["raw"]},
        {name: "Argon2i",               legalUsages: ["deriveBits", "deriveKey"], extractable: [false],       formats: ["raw-secret"]},
        {name: "Argon2d",               legalUsages: ["deriveBits", "deriveKey"], extractable: [false],       formats: ["raw-secret"]},
        {name: "Argon2id",              legalUsages: ["deriveBits", "deriveKey"], extractable: [false],       formats: ["raw-secret"]},
        {name: "AES-OCB",               legalUsages: ["encrypt", "decrypt"],      extractable: [true, false], formats: ["raw-secret", "jwk"]},
        {name: "ChaCha20-Poly1305",     legalUsages: ["encrypt", "decrypt"],      extractable: [true, false], formats: ["raw-secret", "jwk"]},
        {name: "KMAC128",               legalUsages: ["sign", "verify"],          extractable: [true, false], formats: ["raw-secret", "jwk"]},
        {name: "KMAC256",               legalUsages: ["sign", "verify"],          extractable: [true, false], formats: ["raw-secret", "jwk"]},
    ];



    // TESTS ARE HERE:
    // Test every test vector, along with all available key data
    testVectors.filter(({ name }) => name === algorithmName).forEach(function(vector) {
        var algorithm = {name: vector.name};
        if ("hash" in vector) {
            algorithm.hash = vector.hash;
        }

        rawKeyData.forEach(function(keyData) {
            if (vector.name === 'ChaCha20-Poly1305' && keyData.byteLength !== 32) return;
            // Try each legal value of the extractable parameter
            vector.extractable.forEach(function(extractable) {
                vector.formats.forEach(function(format) {
                    var data = keyData;
                    if (format === "jwk") {
                        data = jwkData(keyData, algorithm);
                    }
                    // Generate all combinations of valid usages for testing
                    allValidUsages(vector.legalUsages).forEach(function(usages) {
                        testFormat(format, algorithm, data, keyData.length * 8, usages, extractable);
                    });
                    testEmptyUsages(format, algorithm, data, keyData.length * 8, extractable);
                });
            });

        });
    });

    function hasLength(algorithm) {
        return algorithm.name === 'HMAC' || algorithm.name.startsWith('AES') || algorithm.name.startsWith('KMAC');
    }

    // Test importKey with a given key format and other parameters. If
    // extrable is true, export the key and verify that it matches the input.
    function testFormat(format, algorithm, keyData, keySize, usages, extractable) {
        promise_test(function(test) {
            return subtle.importKey(format, keyData, algorithm, extractable, usages).
            then(function(key) {
                assert_equals(key.constructor, CryptoKey, "Imported a CryptoKey object");
                assert_goodCryptoKey(key, hasLength(key.algorithm) ? { length: keySize, ...algorithm } : algorithm, extractable, usages, 'secret');
                if (!extractable) {
                    return;
                }

                return subtle.exportKey(format, key).
                then(function(result) {
                    if (format !== "jwk") {
                        assert_true(equalBuffers(keyData, result), "Round trip works");
                    } else {
                        assert_true(equalJwk(keyData, result), "Round trip works");
                    }
                }, function(err) {
                    assert_unreached("Threw an unexpected error: " + err.toString());
                });
            }, function(err) {
                assert_unreached("Threw an unexpected error: " + err.toString());
            });
        }, "Good parameters: " + keySize.toString() + " bits " + parameterString(format, keyData, algorithm, extractable, usages));
    }

    // Test importKey with a given key format and other parameters but with empty usages.
    // Should fail with SyntaxError
    function testEmptyUsages(format, algorithm, keyData, keySize, extractable) {
        const usages = [];
        promise_test(function(test) {
            return subtle.importKey(format, keyData, algorithm, extractable, usages).
            then(function(key) {
                assert_unreached("importKey succeeded but should have failed with SyntaxError");
            }, function(err) {
                assert_equals(err.name, "SyntaxError", "Should throw correct error, not " + err.name + ": " + err.message);
            });
        }, "Empty Usages: " + keySize.toString() + " bits " + parameterString(format, keyData, algorithm, extractable, usages));
    }



    // Helper methods follow:

    // Build minimal Jwk objects from raw key data and algorithm specifications
    function jwkData(keyData, algorithm) {
        var result = {
            kty: "oct",
            k: byteArrayToUnpaddedBase64(keyData)
        };

        if (algorithm.name.substring(0, 3) === "AES") {
            result.alg = "A" + (8 * keyData.byteLength).toString() + algorithm.name.substring(4);
        } else if (algorithm.name === "HMAC") {
            result.alg = "HS" + algorithm.hash.substring(4);
        } else if (algorithm.name.startsWith("KMAC")) {
            result.alg = "K" + algorithm.name.substring(4);
        }
        return result;
    }

    // Convert method parameters to a string to uniquely name each test
    function parameterString(format, data, algorithm, extractable, usages) {
        var result = "(" +
                        objectToString(format) + ", " +
                        objectToString(data) + ", " +
                        objectToString(algorithm) + ", " +
                        objectToString(extractable) + ", " +
                        objectToString(usages) +
                     ")";

        return result;
    }
}
