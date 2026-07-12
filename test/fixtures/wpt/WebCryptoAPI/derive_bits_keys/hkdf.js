function define_tests() {
    return runKdfTests({
        name: "HKDF",
        getBaseKeyData: function(testData) {
            return testData.derivedKeys;
        },
        registerTests: function(context) {
            var subtle = context.subtle;
            var testData = context.testData;
            var derivations = testData.derivations;
            var salts = testData.salts;
            var infos = testData.infos;

            Object.keys(derivations).forEach(function(derivedKeySize) {
                Object.keys(derivations[derivedKeySize]).forEach(function(saltSize) {
                    Object.keys(derivations[derivedKeySize][saltSize]).forEach(function(hashName) {
                        Object.keys(derivations[derivedKeySize][saltSize][hashName]).forEach(function(infoSize) {
                            var testName = derivedKeySize + " derivedKey, " + saltSize + " salt, " + hashName + ", with " + infoSize + " info";
                            var testCase = {
                                name: testName,
                                keyName: derivedKeySize,
                                hash: hashName,
                                algorithm: {name: "HKDF", salt: salts[saltSize], info: infos[infoSize], hash: hashName},
                                expected: derivations[derivedKeySize][saltSize][hashName][infoSize]
                            };

                            context.registerCase(testCase, function() {
                                subsetTest(promise_test, function(test) {
                                    return subtle.deriveBits({name: "HKDF", info: infos[infoSize], hash: hashName}, context.keys.baseKeys[derivedKeySize], 0)
                                    .then(function(derivation) {
                                        assert_equals(derivation.byteLength, 0, "Derived even with missing salt");
                                    }, function(err) {
                                        assert_equals(err.name, "TypeError", "deriveBits missing salt correctly threw OperationError: " + err.message);
                                    });
                                }, testName + " with missing salt");

                                subsetTest(promise_test, function(test) {
                                    return subtle.deriveBits({name: "HKDF", salt: salts[saltSize], hash: hashName}, context.keys.baseKeys[derivedKeySize], 0)
                                    .then(function(derivation) {
                                        assert_equals(derivation.byteLength, 0, "Derived even with missing info");
                                    }, function(err) {
                                        assert_equals(err.name, "TypeError", "deriveBits missing info correctly threw OperationError: " + err.message);
                                    });
                                }, testName + " with missing info");
                            });
                        });
                    });

                    var nonDigestHash = "PBKDF2";
                    Object.keys(infos).forEach(function(infoSize) {
                        var testName = derivedKeySize + " derivedKey, " + saltSize + " salt, " + nonDigestHash + ", with " + infoSize + " info";
                        context.registerNonDigestCase({
                            name: testName,
                            keyName: derivedKeySize,
                            hash: nonDigestHash,
                            algorithm: {name: "HKDF", salt: salts[saltSize], hash: nonDigestHash, info: infos[infoSize]}
                        });
                    });
                });
            });
        }
    });
}
