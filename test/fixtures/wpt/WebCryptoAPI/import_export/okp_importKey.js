var subtle = crypto.subtle;

function runTests(algorithmName) {
    var algorithm = {name: algorithmName};
    var data = keyData[algorithmName];
    var jwkData = {jwk: {kty: data.jwk.kty, crv: data.jwk.crv, x: data.jwk.x}};

    [true, false].forEach(function(extractable) {
        // Test public keys first
        allValidUsages(data.publicUsages, true).forEach(function(usages) {
            ['spki', 'jwk', 'raw'].forEach(function(format) {
                if (format === "jwk") { // Not all fields used for public keys
                    testFormat(format, algorithm, jwkData, algorithmName, usages, extractable);
                    // Test for https://github.com/w3c/webcrypto/pull/401
                    if (extractable) {
                        testJwkAlgBehaviours(algorithm, jwkData.jwk, algorithmName, usages);
                    }
                } else {
                    testFormat(format, algorithm, data, algorithmName, usages, extractable);
                }
            });

        });

        // Next, test private keys
        allValidUsages(data.privateUsages).forEach(function(usages) {
            ['pkcs8', 'jwk'].forEach(function(format) {
                testFormat(format, algorithm, data, algorithmName, usages, extractable);

                // Test for https://github.com/w3c/webcrypto/pull/401
                if (format === "jwk" && extractable) {
                    testJwkAlgBehaviours(algorithm, data.jwk, algorithmName, usages);
                }
            });
        });
    });
}


// Test importKey with a given key format and other parameters. If
// extrable is true, export the key and verify that it matches the input.
function testFormat(format, algorithm, keyData, keySize, usages, extractable) {
    [algorithm, algorithm.name].forEach((alg) => {
        promise_test(function(test) {
            return subtle.importKey(format, keyData[format], alg, extractable, usages).
                then(function(key) {
                    assert_equals(key.constructor, CryptoKey, "Imported a CryptoKey object");
                    assert_goodCryptoKey(key, algorithm, extractable, usages, (format === 'pkcs8' || (format === 'jwk' && keyData[format].d)) ? 'private' : 'public');
                    if (!extractable) {
                        return;
                    }

                    return subtle.exportKey(format, key).
                        then(function(result) {
                            if (format !== "jwk") {
                                assert_true(equalBuffers(keyData[format], result), "Round trip works");
                            } else {
                                assert_true(equalJwk(keyData[format], result), "Round trip works");
                            }
                        }, function(err) {
                            assert_unreached("Threw an unexpected error: " + err.toString());
                        });
                }, function(err) {
                    assert_unreached("Threw an unexpected error: " + err.toString());
                });
        }, "Good parameters: " + keySize.toString() + " bits " + parameterString(format, keyData[format], alg, extractable, usages));
    });
}

// Test importKey/exportKey "alg" behaviours (https://github.com/w3c/webcrypto/pull/401)
// - alg is ignored for ECDH import
// - TODO: alg is checked to be the algorithm.name or EdDSA for Ed25519 and Ed448 import
// - alg is missing for ECDH export
// - alg is the algorithm name for Ed25519 and Ed448 export
function testJwkAlgBehaviours(algorithm, keyData, crv, usages) {
    [algorithm, algorithm.name].forEach((alg) => {
        (crv.startsWith('Ed') ? [algorithm.name, 'EdDSA'] : ['this is ignored']).forEach((jwkAlg) => {
            promise_test(function(test) {
                return subtle.importKey('jwk', { ...keyData, alg: jwkAlg }, alg, true, usages).
                    then(function(key) {
                        assert_equals(key.constructor, CryptoKey, "Imported a CryptoKey object");

                        return subtle.exportKey('jwk', key).
                            then(function(result) {
                                let expectedKeys = crv.startsWith('Ed') ? 6 : 5
                                if (keyData.d) expectedKeys++
                                assert_equals(Object.keys(result).length, expectedKeys, "Correct number of JWK members");
                                assert_equals(result.alg, crv.startsWith('Ed') ? algorithm.name : undefined, 'Expected JWK "alg" member');
                                assert_true(equalJwk(keyData, result), "Round trip works");
                            }, function(err) {
                            assert_unreached("Threw an unexpected error: " + err.toString());
                            });
                    }, function(err) {
                        assert_unreached("Threw an unexpected error: " + err.toString());
                    });
            }, 'Good parameters with JWK alg' + (crv.startsWith('Ed') ? ` ${jwkAlg}: ` : ': ') + crv.toString() + " " + parameterString('jwk', keyData, alg, true, usages, jwkAlg));
        });
    });
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
function parameterString(format, data, algorithm, extractable, usages) {
    if ("byteLength" in data) {
        data = "buffer(" + data.byteLength.toString() + ")";
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
