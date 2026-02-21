
function run_test() {
    setup({explicit_done: true});

    var subtle = self.crypto.subtle; // Change to test prefixed implementations

    // When are all these tests really done? When all the promises they use have resolved.
    var all_promises = [];

    // Source file [algorithm_name]_vectors.js provides the getTestVectors method
    // for the algorithm that drives these tests.
    var testVectors = getTestVectors();

    // Test verification first, because signing tests rely on that working
    testVectors.forEach(function(vector) {
        var promise = importVectorKeys(vector, ["verify"], ["sign"])
        .then(function(vectors) {
            promise_test(function(test) {
                var operation = subtle.verify(vector.algorithm, vector.publicKey, vector.signature, vector.plaintext)
                .then(function(is_verified) {
                    assert_true(is_verified, "Signature verified");
                }, function(err) {
                    assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
                });

                return operation;
            }, vector.name + " verification");

        }, function(err) {
            // We need a failed test if the importVectorKey operation fails, so
            // we know we never tested verification.
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " verification");
        });

        all_promises.push(promise);
    });

    // Test verification with an altered buffer after call
    testVectors.forEach(function(vector) {
        var promise = importVectorKeys(vector, ["verify"], ["sign"])
        .then(function(vectors) {
            promise_test(function(test) {
                var signature = copyBuffer(vector.signature);
                var operation = subtle.verify(vector.algorithm, vector.publicKey, signature, vector.plaintext)
                .then(function(is_verified) {
                    assert_true(is_verified, "Signature verified");
                }, function(err) {
                    assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
                });

                signature[0] = 255 - signature[0];
                return operation;
            }, vector.name + " verification with altered signature after call");
        }, function(err) {
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " verification with altered signature after call");
        });

        all_promises.push(promise);
    });

    // Check for successful verification even if plaintext is altered after call.
    testVectors.forEach(function(vector) {
        var promise = importVectorKeys(vector, ["verify"], ["sign"])
        .then(function(vectors) {
            promise_test(function(test) {
                var plaintext = copyBuffer(vector.plaintext);
                var operation = subtle.verify(vector.algorithm, vector.publicKey, vector.signature, plaintext)
                .then(function(is_verified) {
                    assert_true(is_verified, "Signature verified");
                }, function(err) {
                    assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
                });

                plaintext[0] = 255 - plaintext[0];
                return operation;
            }, vector.name + " with altered plaintext after call");
        }, function(err) {
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " with altered plaintext after call");
        });

        all_promises.push(promise);
    });

    // Check for failures due to using privateKey to verify.
    testVectors.forEach(function(vector) {
        var promise = importVectorKeys(vector, ["verify"], ["sign"])
        .then(function(vectors) {
            promise_test(function(test) {
                return subtle.verify(vector.algorithm, vector.privateKey, vector.signature, vector.plaintext)
                .then(function(plaintext) {
                    assert_unreached("Should have thrown error for using privateKey to verify in " + vector.name + ": " + err.message + "'");
                }, function(err) {
                    assert_equals(err.name, "InvalidAccessError", "Should throw InvalidAccessError instead of '" + err.message + "'");
                });
            }, vector.name + " using privateKey to verify");

        }, function(err) {
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " using privateKey to verify");
        });

        all_promises.push(promise);
    });

    // Check for failures due to using publicKey to sign.
    testVectors.forEach(function(vector) {
        var promise = importVectorKeys(vector, ["verify"], ["sign"])
        .then(function(vectors) {
            promise_test(function(test) {
                return subtle.sign(vector.algorithm, vector.publicKey, vector.plaintext)
                .then(function(signature) {
                    assert_unreached("Should have thrown error for using publicKey to sign in " + vector.name + ": " + err.message + "'");
                }, function(err) {
                    assert_equals(err.name, "InvalidAccessError", "Should throw InvalidAccessError instead of '" + err.message + "'");
                });
            }, vector.name + " using publicKey to sign");
        }, function(err) {
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " using publicKey to sign");
        });

        all_promises.push(promise);
    });

    // Check for failures due to no "verify" usage.
    testVectors.forEach(function(originalVector) {
        var vector = Object.assign({}, originalVector);

        var promise = importVectorKeys(vector, [], ["sign"])
        .then(function(vectors) {
            promise_test(function(test) {
                return subtle.verify(vector.algorithm, vector.publicKey, vector.signature, vector.plaintext)
                .then(function(plaintext) {
                    assert_unreached("Should have thrown error for no verify usage in " + vector.name + ": " + err.message + "'");
                }, function(err) {
                    assert_equals(err.name, "InvalidAccessError", "Should throw InvalidAccessError instead of '" + err.message + "'");
                });
            }, vector.name + " no verify usage");
        }, function(err) {
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " no verify usage");
        });

        all_promises.push(promise);
    });

    // Check for successful signing and verification.
    testVectors.forEach(function(vector) {
        // RSA signing is deterministic with PKCS#1 v1.5, or PSS with zero-length salts.
        const isDeterministic = !("saltLength" in vector.algorithm) || vector.algorithm.saltLength == 0;
        var promise = importVectorKeys(vector, ["verify"], ["sign"])
        .then(function(vectors) {
            promise_test(function(test) {
                return subtle.sign(vector.algorithm, vector.privateKey, vector.plaintext)
                .then(function(signature) {
                    if (isDeterministic) {
                        // If deterministic, we can check the output matches. Otherwise, we can only check it verifies.
                        assert_true(equalBuffers(signature, vector.signature), "Signing did not give the expected output");
                    }
                    // Can we verify the new signature?
                    return subtle.verify(vector.algorithm, vector.publicKey, signature, vector.plaintext)
                    .then(function(is_verified) {
                        assert_true(is_verified, "Round trip verifies");
                        return signature;
                    }, function(err) {
                        assert_unreached("verify error for test " + vector.name + ": " + err.message + "'");
                    });
                })
                .then(function(priorSignature) {
                    // Will a second signing give us different signature? It should for PSS with non-empty salt
                    return subtle.sign(vector.algorithm, vector.privateKey, vector.plaintext)
                    .then(function(signature) {
                        if (isDeterministic) {
                            assert_true(equalBuffers(priorSignature, signature), "Two signings with empty salt give same signature")
                        } else {
                            assert_false(equalBuffers(priorSignature, signature), "Two signings with a salt give different signatures")
                        }
                    }, function(err) {
                        assert_unreached("second time verify error for test " + vector.name + ": '" + err.message + "'");
                    });
                }, function(err) {
                    assert_unreached("sign error for test " + vector.name + ": '" + err.message + "'");
                });
            }, vector.name + " round trip");

        }, function(err) {
            // We need a failed test if the importVectorKey operation fails, so
            // we know we never tested signing or verifying
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " round trip");
        });

        all_promises.push(promise);
    });


    // Test signing with the wrong algorithm
    testVectors.forEach(function(vector) {
        // Want to get the key for the wrong algorithm
        var alteredVector = Object.assign({}, vector);
        alteredVector.algorithm = Object.assign({}, vector.algorithm);
        if (vector.algorithm.name === "RSA-PSS") {
            alteredVector.algorithm.name = "RSASSA-PKCS1-v1_5";
        } else {
            alteredVector.algorithm.name = "RSA-PSS";
        }

        var promise = importVectorKeys(alteredVector, ["verify"], ["sign"])
        .then(function(vectors) {
            promise_test(function(test) {
                var operation = subtle.sign(vector.algorithm, alteredVector.privateKey, vector.plaintext)
                .then(function(signature) {
                    assert_unreached("Signing should not have succeeded for " + vector.name);
                }, function(err) {
                    assert_equals(err.name, "InvalidAccessError", "Should have thrown InvalidAccessError instead of '" + err.message + "'");
                });

                return operation;
            }, vector.name + " signing with wrong algorithm name");

        }, function(err) {
            // We need a failed test if the importVectorKey operation fails, so
            // we know we never tested verification.
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " signing with wrong algorithm name");
        });

        all_promises.push(promise);
    });

    // Test verification with the wrong algorithm
    testVectors.forEach(function(vector) {
        // Want to get the key for the wrong algorithm
        var alteredVector = Object.assign({}, vector);
        alteredVector.algorithm = Object.assign({}, vector.algorithm);
        if (vector.algorithm.name === "RSA-PSS") {
            alteredVector.algorithm.name = "RSASSA-PKCS1-v1_5";
        } else {
            alteredVector.algorithm.name = "RSA-PSS";
        }

        var promise = importVectorKeys(alteredVector, ["verify"], ["sign"])
        .then(function(vectors) {
            // Some tests are sign only
            if (!("signature" in vector)) {
                return;
            }
            promise_test(function(test) {
                var operation = subtle.verify(vector.algorithm, alteredVector.publicKey, vector.signature, vector.plaintext)
                .then(function(is_verified) {
                    assert_unreached("Verification should not have succeeded for " + vector.name);
                }, function(err) {
                    assert_equals(err.name, "InvalidAccessError", "Should have thrown InvalidAccessError instead of '" + err.message + "'");
                });

                return operation;
            }, vector.name + " verification with wrong algorithm name");

        }, function(err) {
            // We need a failed test if the importVectorKey operation fails, so
            // we know we never tested verification.
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " verification with wrong algorithm name");
        });

        all_promises.push(promise);
    });

    // Verification should fail with wrong signature
    testVectors.forEach(function(vector) {
        var promise = importVectorKeys(vector, ["verify"], ["sign"])
        .then(function(vectors) {
            promise_test(function(test) {
                var signature = copyBuffer(vector.signature);
                signature[0] = 255 - signature[0];
                var operation = subtle.verify(vector.algorithm, vector.publicKey, signature, vector.plaintext)
                .then(function(is_verified) {
                    assert_false(is_verified, "Signature NOT verified");
                }, function(err) {
                    assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
                });

                return operation;
            }, vector.name + " verification failure with altered signature");

        }, function(err) {
            // We need a failed test if the importVectorKey operation fails, so
            // we know we never tested verification.
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " verification failure with altered signature");
        });

        all_promises.push(promise);
    });

    // [RSA-PSS] Verification should fail with wrong saltLength
    testVectors.forEach(function(vector) {
        if (vector.algorithm.name === "RSA-PSS") {
            var promise = importVectorKeys(vector, ["verify"], ["sign"])
            .then(function(vectors) {
                promise_test(function(test) {
                    const saltLength = vector.algorithm.saltLength === 32 ? 48 : 32;
                    var operation = subtle.verify({ ...vector.algorithm, saltLength }, vector.publicKey, vector.signature, vector.plaintext)
                    .then(function(is_verified) {
                        assert_false(is_verified, "Signature NOT verified");
                    }, function(err) {
                        assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
                    });

                    return operation;
                }, vector.name + " verification failure with wrong saltLength");

            }, function(err) {
                // We need a failed test if the importVectorKey operation fails, so
                // we know we never tested verification.
                promise_test(function(test) {
                    assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
                }, "importVectorKeys step: " + vector.name + " verification failure with wrong saltLength");
            });

            all_promises.push(promise);
        }
    });

    // Verification should fail with wrong plaintext
    testVectors.forEach(function(vector) {
        var promise = importVectorKeys(vector, ["verify"], ["sign"])
        .then(function(vectors) {
            promise_test(function(test) {
                var plaintext = copyBuffer(vector.plaintext);
                plaintext[0] = 255 - plaintext[0];
                var operation = subtle.verify(vector.algorithm, vector.publicKey, vector.signature, plaintext)
                .then(function(is_verified) {
                    assert_false(is_verified, "Signature NOT verified");
                }, function(err) {
                    assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
                });

                return operation;
            }, vector.name + " verification failure with altered plaintext");

        }, function(err) {
            // We need a failed test if the importVectorKey operation fails, so
            // we know we never tested verification.
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " verification failure with altered plaintext");
        });

        all_promises.push(promise);
    });


    promise_test(function() {
        return Promise.all(all_promises)
            .then(function() {done();})
            .catch(function() {done();})
    }, "setup");

    // A test vector has all needed fields for signing and verifying, EXCEPT that the
    // key field may be null. This function replaces that null with the Correct
    // CryptoKey object.
    //
    // Returns a Promise that yields an updated vector on success.
    function importVectorKeys(vector, publicKeyUsages, privateKeyUsages) {
        var publicPromise, privatePromise;

        if (vector.publicKey !== null) {
            publicPromise = new Promise(function(resolve, reject) {
                resolve(vector);
            });
        } else {
            publicPromise = subtle.importKey(vector.publicKeyFormat, vector.publicKeyBuffer, {name: vector.algorithm.name, hash: vector.hash}, false, publicKeyUsages)
            .then(function(key) {
                vector.publicKey = key;
                return vector;
            });        // Returns a copy of the sourceBuffer it is sent.
        }

        if (vector.privateKey !== null) {
            privatePromise = new Promise(function(resolve, reject) {
                resolve(vector);
            });
        } else {
            privatePromise = subtle.importKey(vector.privateKeyFormat, vector.privateKeyBuffer, {name: vector.algorithm.name, hash: vector.hash}, false, privateKeyUsages)
            .then(function(key) {
                vector.privateKey = key;
                return vector;
            });
        }

        return Promise.all([publicPromise, privatePromise]);
    }

    // Returns a copy of the sourceBuffer it is sent.
    function copyBuffer(sourceBuffer) {
        var source = new Uint8Array(sourceBuffer);
        var copy = new Uint8Array(sourceBuffer.byteLength)

        for (var i=0; i<source.byteLength; i++) {
            copy[i] = source[i];
        }

        return copy;
    }

    function equalBuffers(a, b) {
        if (a.byteLength !== b.byteLength) {
            return false;
        }

        var aBytes = new Uint8Array(a);
        var bBytes = new Uint8Array(b);

        for (var i=0; i<a.byteLength; i++) {
            if (aBytes[i] !== bBytes[i]) {
                return false;
            }
        }

        return true;
    }

    return;
}
