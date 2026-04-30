
function run_test(algorithmNames, slowTest) {
    var subtle = crypto.subtle; // Change to test prefixed implementations

    setup({explicit_timeout: true});

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

    var allTestVectors = [ // Parameters that should work for generateKey
        {name: "AES-CTR",  resultType: CryptoKey, usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"], mandatoryUsages: []},
        {name: "AES-CBC",  resultType: CryptoKey, usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"], mandatoryUsages: []},
        {name: "AES-GCM",  resultType: CryptoKey, usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"], mandatoryUsages: []},
        {name: "AES-OCB",  resultType: CryptoKey, usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"], mandatoryUsages: []},
        {name: "ChaCha20-Poly1305",  resultType: CryptoKey, usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"], mandatoryUsages: []},
        {name: "AES-KW",   resultType: CryptoKey, usages: ["wrapKey", "unwrapKey"], mandatoryUsages: []},
        {name: "HMAC",     resultType: CryptoKey, usages: ["sign", "verify"], mandatoryUsages: []},
        {name: "RSASSA-PKCS1-v1_5", resultType: "CryptoKeyPair", usages: ["sign", "verify"], mandatoryUsages: ["sign"]},
        {name: "RSA-PSS",  resultType: "CryptoKeyPair", usages: ["sign", "verify"], mandatoryUsages: ["sign"]},
        {name: "RSA-OAEP", resultType: "CryptoKeyPair", usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"], mandatoryUsages: ["decrypt", "unwrapKey"]},
        {name: "ECDSA",    resultType: "CryptoKeyPair", usages: ["sign", "verify"], mandatoryUsages: ["sign"]},
        {name: "ECDH",     resultType: "CryptoKeyPair", usages: ["deriveKey", "deriveBits"], mandatoryUsages: ["deriveKey", "deriveBits"]},
        {name: "Ed25519",  resultType: "CryptoKeyPair", usages: ["sign", "verify"], mandatoryUsages: ["sign"]},
        {name: "Ed448",    resultType: "CryptoKeyPair", usages: ["sign", "verify"], mandatoryUsages: ["sign"]},
        {name: "ML-DSA-44", resultType: "CryptoKeyPair", usages: ["sign", "verify"], mandatoryUsages: ["sign"]},
        {name: "ML-DSA-65", resultType: "CryptoKeyPair", usages: ["sign", "verify"], mandatoryUsages: ["sign"]},
        {name: "ML-DSA-87", resultType: "CryptoKeyPair", usages: ["sign", "verify"], mandatoryUsages: ["sign"]},
        {name: "ML-KEM-512", resultType: "CryptoKeyPair", usages: ["decapsulateBits", "decapsulateKey", "encapsulateBits", "encapsulateKey"], mandatoryUsages: ["decapsulateBits", "decapsulateKey"]},
        {name: "ML-KEM-768", resultType: "CryptoKeyPair", usages: ["decapsulateBits", "decapsulateKey", "encapsulateBits", "encapsulateKey"], mandatoryUsages: ["decapsulateBits", "decapsulateKey"]},
        {name: "ML-KEM-1024", resultType: "CryptoKeyPair", usages: ["decapsulateBits", "decapsulateKey", "encapsulateBits", "encapsulateKey"], mandatoryUsages: ["decapsulateBits", "decapsulateKey"]},
        {name: "X25519",   resultType: "CryptoKeyPair", usages: ["deriveKey", "deriveBits"], mandatoryUsages: ["deriveKey", "deriveBits"]},
        {name: "X448",     resultType: "CryptoKeyPair", usages: ["deriveKey", "deriveBits"], mandatoryUsages: ["deriveKey", "deriveBits"]},
        {name: "KMAC128",  resultType: CryptoKey, usages: ["sign", "verify"], mandatoryUsages: []},
        {name: "KMAC256",  resultType: CryptoKey, usages: ["sign", "verify"], mandatoryUsages: []},
    ];

    var testVectors = [];
    if (algorithmNames && !Array.isArray(algorithmNames)) {
        algorithmNames = [algorithmNames];
    };
    allTestVectors.forEach(function(vector) {
        if (!algorithmNames || algorithmNames.includes(vector.name)) {
            testVectors.push(vector);
        }
    });

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
