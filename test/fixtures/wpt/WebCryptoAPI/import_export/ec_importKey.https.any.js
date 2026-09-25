// META: title=WebCryptoAPI: importKey() for EC keys
// META: timeout=long
// META: script=../util/helpers.js
// META: script=../util/ec_key_fixtures.js

// Test importKey and exportKey for EC algorithms. Only "happy paths" are
// currently tested - those where the operation should succeed.

    var subtle = crypto.subtle;

    var curves = ['P-256', 'P-384', 'P-521'];

    var keyData = ecKeyData;

    // combinations to test
    var testVectors = [
        {name: "ECDSA", privateUsages: ["sign"], publicUsages: ["verify"]},
        {name: "ECDH",  privateUsages: ["deriveKey", "deriveBits"], publicUsages: []}
    ];

    // TESTS ARE HERE:
    // Test every test vector, along with all available key data
    testVectors.forEach(function(vector) {
        curves.forEach(function(curve) {

            [true, false].forEach(function(extractable) {

                // Test public keys first
                allValidUsages(vector.publicUsages, true).forEach(function(usages) {
                    ['spki', 'spki_compressed', 'jwk', 'raw', 'raw_compressed'].forEach(function(format) {
                        var algorithm = {name: vector.name, namedCurve: curve};
                        var data = keyData[curve];
                        if (format === "jwk") { // Not all fields used for public keys
                            data = {jwk: {kty: keyData[curve].jwk.kty, crv: keyData[curve].jwk.crv, x: keyData[curve].jwk.x, y: keyData[curve].jwk.y}};
                        }

                        testFormat(format, algorithm, data, curve, usages, extractable);
                        if (vector.name === 'ECDH' && format === 'jwk') {
                            testEcdhJwkAlg(algorithm, { ...data.jwk, alg: 'any alg works here' }, curve, usages, extractable);
                        }
                    });

                });

                // Next, test private keys
                ['pkcs8', 'jwk'].forEach(function(format) {
                    var algorithm = {name: vector.name, namedCurve: curve};
                    var data = keyData[curve];
                    allValidUsages(vector.privateUsages).forEach(function(usages) {
                        testFormat(format, algorithm, data, curve, usages, extractable);
                        if (vector.name === 'ECDH' && format === 'jwk') {
                            testEcdhJwkAlg(algorithm, { ...data.jwk, alg: 'any alg works here' }, curve, usages, extractable);
                        }
                    });
                    testEmptyUsages(format, algorithm, data, curve, extractable);
                });
            });

            {
                var algorithm = {name: vector.name, namedCurve: curve};
                var data = keyData[curve];
                allValidUsages(vector.privateUsages).forEach(function(usages) {
                    testPkcs8PrivateOnly(algorithm, data, curve, usages);
                });
            }
        });
    });


    // Test importKey with a given key format and other parameters. If
    // extrable is true, export the key and verify that it matches the input.
    function testFormat(format, algorithm, data, keySize, usages, extractable) {
        const keyData = data[format];
        const compressed = format.endsWith("_compressed");
        if (compressed) {
            [format] = format.split("_compressed");
        }
        promise_test(function(test) {
            return subtle.importKey(format, keyData, algorithm, extractable, usages).
            then(function(key) {
                assert_equals(key.constructor, CryptoKey, "Imported a CryptoKey object");
                assert_goodCryptoKey(key, algorithm, extractable, usages, (format === 'pkcs8' || (format === 'jwk' && keyData.d)) ? 'private' : 'public');
                if (!extractable) {
                    return;
                }

                return subtle.exportKey(format, key).
                then(function(result) {
                    if (format !== "jwk") {
                        assert_true(equalBuffers(data[format], result), "Round trip works");
                    } else {
                        assert_true(equalJwk(data[format], result), "Round trip works");
                    }
                }, function(err) {
                    assert_unreached("Threw an unexpected error: " + err.toString());
                });
            }, function(err) {
                if (compressed && err.name === "DataError") {
                    assert_implements_optional(false, "Compressed point format not supported: " + err.toString());
                } else {
                    assert_unreached("Threw an unexpected error: " + err.toString());
                }
            });
        }, "Good parameters: " + keySize.toString() + " bits " + parameterString(format, compressed, keyData, algorithm, extractable, usages));
    }

    // Test importKey with a given key format and other parameters but with empty usages.
    // Should fail with SyntaxError
    function testEmptyUsages(format, algorithm, data, keySize, extractable) {
        const keyData = data[format];
        const usages = [];
        promise_test(function(test) {
            return subtle.importKey(format, keyData, algorithm, extractable, usages).
            then(function(key) {
                assert_unreached("importKey succeeded but should have failed with SyntaxError");
            }, function(err) {
                assert_equals(err.name, "SyntaxError", "Should throw correct error, not " + err.name + ": " + err.message);
            });
        }, "Empty Usages: " + keySize.toString() + " bits " + parameterString(format, false, keyData, algorithm, extractable, usages));
    }

    // Test ECDH importKey with a JWK format
    // Should succeed with any "alg" value
    function testEcdhJwkAlg(algorithm, keyData, keySize, usages, extractable) {
        const format = "jwk";
        promise_test(function(test) {
            return subtle.importKey(format, keyData, algorithm, extractable, usages).
            then(function(key) {
                assert_equals(key.constructor, CryptoKey, "Imported a CryptoKey object");
                assert_goodCryptoKey(key, algorithm, extractable, usages, keyData.d ? 'private' : 'public');
            }, function(err) {
                assert_unreached("Threw an unexpected error: " + err.toString());
            });
        }, "ECDH any JWK alg: " + keySize.toString() + " bits " + parameterString(format, false, keyData, algorithm, extractable, usages));
    }

    // Test importKey with pkcs8 without public key
    // Should succeed in import, re-export as pkcs8, and export as jwk
    function testPkcs8PrivateOnly(algorithm, data, keySize, usages) {
        const format = "pkcs8";
        const keyData = data.pkcs8_private_only;
        promise_test(function(test) {
            return subtle.importKey(format, keyData, algorithm, true, usages).
            then(function(key) {
                assert_equals(key.constructor, CryptoKey, "Imported a CryptoKey object");
                assert_goodCryptoKey(key, algorithm, true, usages, 'private');

                // Test re-export as pkcs8
                return subtle.exportKey(format, key).
                then(function(result) {
                    assert_true(result instanceof ArrayBuffer, "Re-exported pkcs8 is an ArrayBuffer");
                    const equal = equalBuffers(result, keyData.buffer) || equalBuffers(result, data.pkcs8.buffer);
                    assert_true(equal, "Round trip works");
                    // Test export as jwk
                    return subtle.exportKey('jwk', key);
                }).then(function(jwkResult) {
                    assert_equals(jwkResult.kty, "EC", "Exported JWK has correct kty");
                    assert_equals(jwkResult.crv, algorithm.namedCurve, "Exported JWK has correct crv");
                    assert_true('x' in jwkResult, "Exported JWK has x");
                    assert_true('y' in jwkResult, "Exported JWK has y");
                    assert_true('d' in jwkResult, "Exported JWK has d");
                }, function(err) {
                    assert_unreached("Export threw an unexpected error: " + err.toString());
                });
            }, function(err) {
                assert_unreached("Import threw an unexpected error: " + err.toString());
            });
        }, "PKCS8 private-only: " + keySize.toString() + " bits " + parameterString(format, false, keyData, algorithm, true, usages));
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
        }
        return result;
    }

    // Convert method parameters to a string to uniquely name each test
    function parameterString(format, compressed, data, algorithm, extractable, usages) {
        if ("byteLength" in data) {
            data = "buffer(" + data.byteLength.toString() + (compressed ? ", compressed" : "") + ")";
        } else {
            data = "object(" + Object.keys(data).join(", ") + ")";
        }
        var result = "(" +
                        objectToString(format) + ", " +
                        objectToString(data) + ", " +
                        objectToString(algorithm) + ", " +
                        objectToString(extractable) + ", " +
                        objectToString(usages) +
                     ")";

        return result;
    }
