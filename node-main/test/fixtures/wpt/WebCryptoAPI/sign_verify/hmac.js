
function run_test() {
    setup({explicit_done: true});

    var subtle = self.crypto.subtle; // Change to test prefixed implementations

    // When are all these tests really done? When all the promises they use have resolved.
    var all_promises = [];

    // Source file hmac_vectors.js provides the getTestVectors method
    // for the algorithm that drives these tests.
    var testVectors = getTestVectors();

    // Test verification first, because signing tests rely on that working
    testVectors.forEach(function(vector) {
        var promise = importVectorKeys(vector, ["verify", "sign"])
        .then(function(vector) {
            promise_test(function(test) {
                var operation = subtle.verify({name: "HMAC", hash: vector.hash}, vector.key, vector.signature, vector.plaintext)
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
        var promise = importVectorKeys(vector, ["verify", "sign"])
        .then(function(vector) {
            promise_test(function(test) {
                var signature = copyBuffer(vector.signature);
                var operation = subtle.verify({name: "HMAC", hash: vector.hash}, vector.key, signature, vector.plaintext)
                .then(function(is_verified) {
                    assert_true(is_verified, "Signature is not verified");
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
        var promise = importVectorKeys(vector, ["verify", "sign"])
        .then(function(vector) {
            promise_test(function(test) {
                var plaintext = copyBuffer(vector.plaintext);
                var operation = subtle.verify({name: "HMAC", hash: vector.hash}, vector.key, vector.signature, plaintext)
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
            }, "importVectorKeys step: " + vector.name + " with altered plaintext");
        });

        all_promises.push(promise);
    });

    // Check for failures due to no "verify" usage.
    testVectors.forEach(function(originalVector) {
        var vector = Object.assign({}, originalVector);

        var promise = importVectorKeys(vector, ["sign"])
        .then(function(vector) {
            promise_test(function(test) {
                return subtle.verify({name: "HMAC", hash: vector.hash}, vector.key, vector.signature, vector.plaintext)
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
        var promise = importVectorKeys(vector, ["verify", "sign"])
        .then(function(vectors) {
            promise_test(function(test) {
                return subtle.sign({name: "HMAC", hash: vector.hash}, vector.key, vector.plaintext)
                .then(function(signature) {
                    assert_true(equalBuffers(signature, vector.signature), "Signing did not give the expected output");
                    // Can we get the verify the new signature?
                    return subtle.verify({name: "HMAC", hash: vector.hash}, vector.key, signature, vector.plaintext)
                    .then(function(is_verified) {
                        assert_true(is_verified, "Round trip verifies");
                        return signature;
                    }, function(err) {
                        assert_unreached("verify error for test " + vector.name + ": " + err.message + "'");
                    });
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
        var promise = subtle.generateKey({name: "ECDSA", namedCurve: "P-256", hash: "SHA-256"}, false, ["sign", "verify"])
        .then(function(wrongKey) {
            return importVectorKeys(vector, ["verify", "sign"])
            .then(function(vectors) {
                promise_test(function(test) {
                    var operation = subtle.sign({name: "HMAC", hash: vector.hash}, wrongKey.privateKey, vector.plaintext)
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
        }, function(err) {
            promise_test(function(test) {
                assert_unreached("Generate wrong key for test " + vector.name + " failed: '" + err.message + "'");
            }, "generate wrong key step: " + vector.name + " signing with wrong algorithm name");
        });

        all_promises.push(promise);
    });

    // Test verification with the wrong algorithm
    testVectors.forEach(function(vector) {
        // Want to get the key for the wrong algorithm
        var promise = subtle.generateKey({name: "ECDSA", namedCurve: "P-256", hash: "SHA-256"}, false, ["sign", "verify"])
        .then(function(wrongKey) {
            return importVectorKeys(vector, ["verify", "sign"])
            .then(function(vector) {
                promise_test(function(test) {
                    var operation = subtle.verify({name: "HMAC", hash: vector.hash}, wrongKey.publicKey, vector.signature, vector.plaintext)
                    .then(function(signature) {
                        assert_unreached("Verifying should not have succeeded for " + vector.name);
                    }, function(err) {
                        assert_equals(err.name, "InvalidAccessError", "Should have thrown InvalidAccessError instead of '" + err.message + "'");
                    });

                    return operation;
                }, vector.name + " verifying with wrong algorithm name");

            }, function(err) {
                // We need a failed test if the importVectorKey operation fails, so
                // we know we never tested verification.
                promise_test(function(test) {
                    assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
                }, "importVectorKeys step: " + vector.name + " verifying with wrong algorithm name");
            });
        }, function(err) {
            promise_test(function(test) {
                assert_unreached("Generate wrong key for test " + vector.name + " failed: '" + err.message + "'");
            }, "generate wrong key step: " + vector.name + " verifying with wrong algorithm name");
        });

        all_promises.push(promise);
    });

    // Verification should fail if the plaintext is changed
    testVectors.forEach(function(vector) {
        var promise = importVectorKeys(vector, ["verify", "sign"])
        .then(function(vector) {
            var plaintext = copyBuffer(vector.plaintext);
            plaintext[0] = 255 - plaintext[0];
            promise_test(function(test) {
                var operation = subtle.verify({name: "HMAC", hash: vector.hash}, vector.key, vector.signature, plaintext)
                .then(function(is_verified) {
                    assert_false(is_verified, "Signature is NOT verified");
                }, function(err) {
                    assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
                });

                return operation;
            }, vector.name + " verification failure due to wrong plaintext");

        }, function(err) {
            // We need a failed test if the importVectorKey operation fails, so
            // we know we never tested verification.
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " verification failure due to wrong plaintext");
        });

        all_promises.push(promise);
    });

    // Verification should fail if the signature is changed
    testVectors.forEach(function(vector) {
        var promise = importVectorKeys(vector, ["verify", "sign"])
        .then(function(vector) {
            var signature = copyBuffer(vector.signature);
            signature[0] = 255 - signature[0];
            promise_test(function(test) {
                var operation = subtle.verify({name: "HMAC", hash: vector.hash}, vector.key, signature, vector.plaintext)
                .then(function(is_verified) {
                    assert_false(is_verified, "Signature is NOT verified");
                }, function(err) {
                    assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
                });

                return operation;
            }, vector.name + " verification failure due to wrong signature");

        }, function(err) {
            // We need a failed test if the importVectorKey operation fails, so
            // we know we never tested verification.
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " verification failure due to wrong signature");
        });

        all_promises.push(promise);
    });

    // Verification should fail if the signature is wrong length
    testVectors.forEach(function(vector) {
        var promise = importVectorKeys(vector, ["verify", "sign"])
        .then(function(vector) {
            var signature = vector.signature.slice(1); // Drop first byte
            promise_test(function(test) {
                var operation = subtle.verify({name: "HMAC", hash: vector.hash}, vector.key, signature, vector.plaintext)
                .then(function(is_verified) {
                    assert_false(is_verified, "Signature is NOT verified");
                }, function(err) {
                    assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
                });

                return operation;
            }, vector.name + " verification failure due to short signature");

        }, function(err) {
            // We need a failed test if the importVectorKey operation fails, so
            // we know we never tested verification.
            promise_test(function(test) {
                assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
            }, "importVectorKeys step: " + vector.name + " verification failure due to short signature");
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
    function importVectorKeys(vector, keyUsages) {
        if (vector.key !== null) {
            return new Promise(function(resolve, reject) {
                resolve(vector);
            });
        } else {
            return subtle.importKey("raw", vector.keyBuffer, {name: "HMAC", hash: vector.hash}, false, keyUsages)
            .then(function(key) {
                vector.key = key;
                return vector;
            });
        }
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
