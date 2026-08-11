
function run_test(algorithmNames, slowTest) {
    var subtle = crypto.subtle; // Change to test prefixed implementations

// These tests check that generateKey successfully creates keys
// when provided any of a wide set of correct parameters
// and that they can be exported afterwards.
//
// There are a lot of combinations of possible parameters,
// resulting in a very large number of tests
// performed.


// Setup: define the correct behaviors that should be sought, and create
// helper functions that generate all possible test parameters for
// different situations.

    var testVectors = getGenerateKeyTestVectors(algorithmNames);

    function parameterString(algorithm, extractable, usages) {
        var result = "(" +
                        objectToString(algorithm) + ", " +
                        objectToString(extractable) + ", " +
                        objectToString(usages) +
                     ")";

        return result;
    }

    // Test that a given combination of parameters is successful
    function testSuccess(algorithm, extractable, usages, resultType, testTag) {
        // algorithm, extractable, and usages are the generateKey parameters
        // resultType is the expected result, either the CryptoKey object or "CryptoKeyPair"
        // testTag is a string to prepend to the test name.

        promise_test(function(test) {
            return subtle.generateKey(algorithm, extractable, usages)
            .then(function(result) {
                if (resultType === "CryptoKeyPair") {
                    assert_goodCryptoKey(result.privateKey, algorithm, extractable, usages, "private");
                    assert_goodCryptoKey(result.publicKey, algorithm, true, usages, "public");
                } else {
                    assert_goodCryptoKey(result, algorithm, extractable, usages, "secret");
                }
                return result;
            }, function(err) {
                assert_unreached("generateKey threw an unexpected error: " + err.toString());
            })
            .then(async function (result) {
                // TODO: remove this block to enable ML-KEM JWK when its definition is done in IETF JOSE WG
                if (result.publicKey?.algorithm.name.startsWith('ML-KEM')) {
                    const promises = [
                        subtle.exportKey('spki', result.publicKey),
                        extractable ? subtle.exportKey('pkcs8', result.privateKey) : undefined,
                        subtle.exportKey('raw-public', result.publicKey),
                    ];
                    if (extractable)
                        promises.push(subtle.exportKey('raw-seed', result.privateKey));
                    await Promise.all(promises);
                } else if (resultType === "CryptoKeyPair") {
                    const promises = [
                        subtle.exportKey('jwk', result.publicKey),
                        extractable ? subtle.exportKey('jwk', result.privateKey) : undefined,
                        subtle.exportKey('spki', result.publicKey),
                        extractable ? subtle.exportKey('pkcs8', result.privateKey) : undefined,
                    ];

                    switch (result.publicKey.algorithm.name.substring(0, 2)) {
                        case 'ML':
                            promises.push(subtle.exportKey('raw-public', result.publicKey));
                            if (extractable)
                                promises.push(subtle.exportKey('raw-seed', result.privateKey));
                            break;
                        case 'SL':
                            promises.push(subtle.exportKey('raw-public', result.publicKey));
                            if (extractable)
                                promises.push(subtle.exportKey('raw-private', result.privateKey));
                            break;
                        case 'EC':
                        case 'Ed':
                        case 'X2':
                        case 'X4':
                            promises.push(subtle.exportKey('raw', result.publicKey));
                            break;
                        case 'RS':
                            break;
                        default:
                            throw new Error('not implemented');
                    }

                    const [jwkPub, jwkPriv] = await Promise.all(promises);

                    if (extractable) {
                        // Test that the JWK public key is a superset of the JWK private key.
                        for (const [prop, value] of Object.entries(jwkPub)) {
                            if (prop !== 'key_ops') {
                                assert_equals(value, jwkPriv[prop], `Property ${prop} is equal in public and private JWK`);
                            }
                        }
                    }
                } else {
                    if (extractable) {
                        await Promise.all([
                            subtle.exportKey(/cha|ocb|kmac/i.test(result.algorithm.name) ? 'raw-secret' : 'raw', result),
                            subtle.exportKey('jwk', result),
                        ]);
                    }
                }
            }, function(err) {
                assert_unreached("exportKey threw an unexpected error: " + err.toString());
            })
        }, testTag + ": generateKey" + parameterString(algorithm, extractable, usages));

        // Special case for ECDH and ECDSA: check that the generated key length is consistent.
        // Particularly for P-521, there is a high risk of the generated key being one byte short
        // if the implementation isn't careful.
        if (algorithm.namedCurve && extractable) {
            promise_test(async function(test) {
                // We run about 20 variants of this test, times 10 key generations below,
                // so this should have a decent chance of catching issues.
                await Promise.all(Array.from({ length: 10 }).map(async () => {
                    const { privateKey, publicKey } = await subtle.generateKey(algorithm, extractable, usages);
                    const [jwkPub, jwkPriv] = await Promise.all([
                        subtle.exportKey('jwk', publicKey),
                        subtle.exportKey('jwk', privateKey),
                    ]);
                    const expectedLength = Math.ceil(Math.ceil(parseInt(algorithm.namedCurve.substring(2)) / 8) * 4/3);
                    assert_equals(jwkPub.x.length, expectedLength, "Public key value x has correct length");
                    assert_equals(jwkPub.y.length, expectedLength, "Public key value y has correct length");
                    assert_equals(jwkPriv.d.length, expectedLength, "Private key value d has correct length");
                }));
            }, testTag + ": generateKey" + parameterString(algorithm, extractable, usages) + " produces consistent length key");
        }
    }

    // Test all valid sets of parameters for successful
    // key generation.
    testVectors.forEach(function(vector) {
        allNameVariants(vector.name, slowTest).forEach(function(name) {
            allAlgorithmSpecifiersFor(name).forEach(function(algorithm) {
                allValidUsages(vector.usages, false, vector.mandatoryUsages).forEach(function(usages) {
                    [false, true].forEach(function(extractable) {
                        subsetTest(testSuccess, algorithm, extractable, usages, vector.resultType, "Success");
                    });
                });
            });
        });
    });

}
