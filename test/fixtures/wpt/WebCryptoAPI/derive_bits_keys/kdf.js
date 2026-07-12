function runKdfTests(options) {
    var subtle = self.crypto.subtle;
    var testData = getTestData();
    var derivedKeyTypes = testData.derivedKeyTypes;

    return setUpBaseKeys(options.getBaseKeyData(testData))
    .then(function(allKeys) {
        function derivedKeyTestName(derivedKeyType, caseName) {
            var testName = "Derived key of type ";
            Object.keys(derivedKeyType.algorithm).forEach(function(prop) {
                testName += prop + ": " + derivedKeyType.algorithm[prop] + " ";
            });
            return testName + " using " + caseName;
        }

        function withHash(algorithm, hash) {
            return Object.assign({}, algorithm, {hash: hash});
        }

        function registerCase(testCase, registerAdditionalBitsTests) {
            var algorithm = testCase.algorithm;
            var baseKey = allKeys.baseKeys[testCase.keyName];
            var badHash = testCase.hash.substring(0, 3) + testCase.hash.substring(4);

            subsetTest(promise_test, function(test) {
                return subtle.deriveBits(algorithm, baseKey, 256)
                .then(function(derivation) {
                    assert_true(equalBuffers(derivation, testCase.expected), "Derived correct key");
                }, function(err) {
                    assert_unreached("deriveBits failed with error " + err.name + ": " + err.message);
                });
            }, testCase.name);

            subsetTest(promise_test, function(test) {
                return subtle.deriveBits(algorithm, baseKey, 0)
                .then(function(derivation) {
                    assert_equals(derivation.byteLength, 0, "Derived correctly empty key");
                }, function(err) {
                    assert_unreached("deriveBits failed with error " + err.name + ": " + err.message);
                });
            }, testCase.name + " with 0 length");

            derivedKeyTypes.forEach(function(derivedKeyType) {
                var testName = derivedKeyTestName(derivedKeyType, testCase.name);

                subsetTest(promise_test, function(test) {
                    return subtle.deriveKey(algorithm, baseKey, derivedKeyType.algorithm, true, derivedKeyType.usages)
                    .then(function(key) {
                        return subtle.exportKey("raw", key)
                        .then(function(buffer) {
                            assert_true(equalBuffers(buffer, testCase.expected.slice(0, derivedKeyType.algorithm.length/8)), "Exported key matches correct value");
                        }, function(err) {
                            assert_unreached("Exporting derived key failed with error " + err.name + ": " + err.message);
                        });
                    }, function(err) {
                        assert_unreached("deriveKey failed with error " + err.name + ": " + err.message);
                    });
                }, testName);

                subsetTest(promise_test, function(test) {
                    return subtle.deriveKey(withHash(algorithm, badHash), baseKey, derivedKeyType.algorithm, true, derivedKeyType.usages)
                    .then(function(key) {
                        assert_unreached("bad hash name should have thrown an NotSupportedError");
                    }, function(err) {
                        assert_equals(err.name, "NotSupportedError", "deriveKey with bad hash name correctly threw NotSupportedError: " + err.message);
                    });
                }, testName + " with bad hash name " + badHash);

                subsetTest(promise_test, function(test) {
                    return subtle.deriveKey(algorithm, allKeys.noKey[testCase.keyName], derivedKeyType.algorithm, true, derivedKeyType.usages)
                    .then(function(key) {
                        assert_unreached("missing deriveKey usage should have thrown an InvalidAccessError");
                    }, function(err) {
                        assert_equals(err.name, "InvalidAccessError", "deriveKey with missing deriveKey usage correctly threw InvalidAccessError: " + err.message);
                    });
                }, testName + " with missing deriveKey usage");

                subsetTest(promise_test, function(test) {
                    return subtle.deriveKey(algorithm, allKeys.wrongKey, derivedKeyType.algorithm, true, derivedKeyType.usages)
                    .then(function(key) {
                        assert_unreached("wrong (ECDH) key should have thrown an InvalidAccessError");
                    }, function(err) {
                        assert_equals(err.name, "InvalidAccessError", "deriveKey with wrong (ECDH) key correctly threw InvalidAccessError: " + err.message);
                    });
                }, testName + " with wrong (ECDH) key");
            });

            if (registerAdditionalBitsTests !== undefined) {
                registerAdditionalBitsTests();
            }

            subsetTest(promise_test, function(test) {
                return subtle.deriveBits(algorithm, baseKey, 44)
                .then(function(derivation) {
                    assert_unreached("non-multiple of 8 length should have thrown an OperationError");
                }, function(err) {
                    assert_equals(err.name, "OperationError", "deriveBits with non-multiple of 8 length correctly threw OperationError: " + err.message);
                });
            }, testCase.name + " with non-multiple of 8 length");

            subsetTest(promise_test, function(test) {
                return subtle.deriveBits(withHash(algorithm, badHash), baseKey, 256)
                .then(function(derivation) {
                    assert_unreached("bad hash name should have thrown an NotSupportedError");
                }, function(err) {
                    assert_equals(err.name, "NotSupportedError", "deriveBits with bad hash name correctly threw NotSupportedError: " + err.message);
                });
            }, testCase.name + " with bad hash name " + badHash);

            subsetTest(promise_test, function(test) {
                return subtle.deriveBits(algorithm, allKeys.noBits[testCase.keyName], 256)
                .then(function(derivation) {
                    assert_unreached("missing deriveBits usage should have thrown an InvalidAccessError");
                }, function(err) {
                    assert_equals(err.name, "InvalidAccessError", "deriveBits with missing deriveBits usage correctly threw InvalidAccessError: " + err.message);
                });
            }, testCase.name + " with missing deriveBits usage");

            subsetTest(promise_test, function(test) {
                return subtle.deriveBits(algorithm, allKeys.wrongKey, 256)
                .then(function(derivation) {
                    assert_unreached("wrong (ECDH) key should have thrown an InvalidAccessError");
                }, function(err) {
                    assert_equals(err.name, "InvalidAccessError", "deriveBits with wrong (ECDH) key correctly threw InvalidAccessError: " + err.message);
                });
            }, testCase.name + " with wrong (ECDH) key");
        }

        function registerNonDigestCase(testCase) {
            var baseKey = allKeys.baseKeys[testCase.keyName];

            subsetTest(promise_test, function(test) {
                return subtle.deriveBits(testCase.algorithm, baseKey, 256)
                .then(function(derivation) {
                    assert_unreached("non-digest algorithm should have thrown an NotSupportedError");
                }, function(err) {
                    assert_equals(err.name, "NotSupportedError", "deriveBits with non-digest algorithm correctly threw NotSupportedError: " + err.message);
                });
            }, testCase.name + " with non-digest algorithm " + testCase.hash);

            derivedKeyTypes.forEach(function(derivedKeyType) {
                subsetTest(promise_test, function(test) {
                    return subtle.deriveKey(testCase.algorithm, baseKey, derivedKeyType.algorithm, true, derivedKeyType.usages)
                    .then(function(derivation) {
                        assert_unreached("non-digest algorithm should have thrown an NotSupportedError");
                    }, function(err) {
                        assert_equals(err.name, "NotSupportedError", "derivekey with non-digest algorithm correctly threw NotSupportedError: " + err.message);
                    });
                }, derivedKeyTestName(derivedKeyType, testCase.name));
            });
        }

        options.registerTests({
            subtle: subtle,
            testData: testData,
            derivedKeyTypes: derivedKeyTypes,
            keys: allKeys,
            registerCase: registerCase,
            registerNonDigestCase: registerNonDigestCase,
            derivedKeyTestName: derivedKeyTestName
        });
    });

    function setUpBaseKeys(baseKeyData) {
        var promises = [];
        var baseKeys = {};
        var noBits = {};
        var noKey = {};
        var wrongKey = null;

        Object.keys(baseKeyData).forEach(function(keyName) {
            var promise = subtle.importKey("raw", baseKeyData[keyName], {name: options.name}, false, ["deriveKey", "deriveBits"])
            .then(function(baseKey) {
                baseKeys[keyName] = baseKey;
            }, function(err) {
                baseKeys[keyName] = null;
            });
            promises.push(promise);

            promise = subtle.importKey("raw", baseKeyData[keyName], {name: options.name}, false, ["deriveBits"])
            .then(function(baseKey) {
                noKey[keyName] = baseKey;
            }, function(err) {
                noKey[keyName] = null;
            });
            promises.push(promise);

            promise = subtle.importKey("raw", baseKeyData[keyName], {name: options.name}, false, ["deriveKey"])
            .then(function(baseKey) {
                noBits[keyName] = baseKey;
            }, function(err) {
                noBits[keyName] = null;
            });
            promises.push(promise);
        });

        var promise = subtle.generateKey({name: "ECDH", namedCurve: "P-256"}, false, ["deriveKey", "deriveBits"])
        .then(function(baseKey) {
            wrongKey = baseKey.privateKey;
        }, function(err) {
            wrongKey = null;
        });
        promises.push(promise);

        return Promise.all(promises).then(function() {
            return {baseKeys: baseKeys, noBits: noBits, noKey: noKey, wrongKey: wrongKey};
        });
    }
}
