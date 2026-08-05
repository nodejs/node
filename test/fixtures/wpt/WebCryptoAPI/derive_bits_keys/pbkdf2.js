function define_tests() {
    return runKdfTests({
        name: "PBKDF2",
        getBaseKeyData: function(testData) {
            return testData.passwords;
        },
        registerTests: function(context) {
            var subtle = context.subtle;
            var testData = context.testData;
            var derivations = testData.derivations;
            var salts = testData.salts;

            Object.keys(derivations).forEach(function(passwordSize) {
                Object.keys(derivations[passwordSize]).forEach(function(saltSize) {
                    Object.keys(derivations[passwordSize][saltSize]).forEach(function(hashName) {
                        Object.keys(derivations[passwordSize][saltSize][hashName]).forEach(function(iterations) {
                            var testName = passwordSize + " password, " + saltSize + " salt, " + hashName + ", with " + iterations + " iterations";
                            context.registerCase({
                                name: testName,
                                keyName: passwordSize,
                                hash: hashName,
                                algorithm: {name: "PBKDF2", salt: salts[saltSize], hash: hashName, iterations: parseInt(iterations)},
                                expected: derivations[passwordSize][saltSize][hashName][iterations]
                            });
                        });

                        var zeroIterationName = passwordSize + " password, " + saltSize + " salt, " + hashName + ", with 0 iterations";
                        var zeroIterationAlgorithm = {name: "PBKDF2", salt: salts[saltSize], hash: hashName, iterations: 0};
                        subsetTest(promise_test, function(test) {
                            return subtle.deriveBits(zeroIterationAlgorithm, context.keys.baseKeys[passwordSize], 256)
                            .then(function(derivation) {
                                assert_unreached("0 iterations should have thrown an error");
                            }, function(err) {
                                assert_equals(err.name, "OperationError", "deriveBits with 0 iterations correctly threw OperationError: " + err.message);
                            });
                        }, zeroIterationName);

                        context.derivedKeyTypes.forEach(function(derivedKeyType) {
                            subsetTest(promise_test, function(test) {
                                return subtle.deriveKey(zeroIterationAlgorithm, context.keys.baseKeys[passwordSize], derivedKeyType.algorithm, true, derivedKeyType.usages)
                                .then(function(derivation) {
                                    assert_unreached("0 iterations should have thrown an error");
                                }, function(err) {
                                    assert_equals(err.name, "OperationError", "derivekey with 0 iterations correctly threw OperationError: " + err.message);
                                });
                            }, context.derivedKeyTestName(derivedKeyType, zeroIterationName));
                        });
                    });

                    var nonDigestHash = "PBKDF2";
                    [1, 1000, 100000].forEach(function(iterations) {
                        var testName = passwordSize + " password, " + saltSize + " salt, " + nonDigestHash + ", with " + iterations + " iterations";
                        context.registerNonDigestCase({
                            name: testName,
                            keyName: passwordSize,
                            hash: nonDigestHash,
                            algorithm: {name: "PBKDF2", salt: salts[saltSize], hash: nonDigestHash, iterations: parseInt(iterations)}
                        });
                    });
                });
            });
        }
    });
}
