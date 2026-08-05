function registerDeriveTests(options) {
    const {
        operation,
        algorithmName,
        testName = algorithmName,
        mixedCaseName,
        size,
        derivation,
        privateKey,
        publicKey,
        noUsagePrivateKey,
        invalidPublicKeys,
        missingPublicLabel,
    } = options;
    const subtle = self.crypto.subtle;
    const isDeriveBits = operation === "deriveBits";
    const derivedKeyAlgorithm = {name: "HMAC", hash: "SHA-256", length: 256};
    const derivedKeyUsages = ["sign", "verify"];

    function derive(name, publicKey, baseKey, length = 8 * size, keyOptions = {}) {
        const algorithm = {name, public: publicKey};
        if (isDeriveBits) {
            return subtle.deriveBits(algorithm, baseKey, length);
        }

        return subtle.deriveKey(
            algorithm,
            baseKey,
            keyOptions.algorithm || derivedKeyAlgorithm,
            true,
            keyOptions.usages || derivedKeyUsages
        ).then(key => subtle.exportKey("raw", key));
    }

    function assertDerived(result, length) {
        if (isDeriveBits) {
            assert_true(equalBuffers(result, derivation, length), "Derived correct bits");
        } else {
            assert_array_equals(
                new Uint8Array(result),
                derivation.slice(0, 32),
                "Derived correct key"
            );
        }
    }

    function successTest(name, algorithm, length = 8 * size) {
        promise_test(() => {
            return derive(algorithm, publicKey, privateKey, length).then(
                result => assertDerived(result, length),
                error => assert_unreached(
                    operation + " failed with error " + error.name + ": " + error.message
                )
            );
        }, name);
    }

    function failureTest(name, expectedError, deriveOperation) {
        promise_test(() => {
            return Promise.resolve().then(deriveOperation).then(
                () => assert_unreached(
                    operation + " succeeded but should have failed with " + expectedError
                ),
                error => assert_equals(
                    error.name,
                    expectedError,
                    "Should throw correct error, not " + error.name + ": " + error.message
                )
            );
        }, name);
    }

    successTest(testName + " good parameters", algorithmName);
    successTest(testName + " mixed case parameters", mixedCaseName);

    if (isDeriveBits) {
        successTest(testName + " short result", algorithmName, 8 * size - 32);
        successTest(
            testName + " non-multiple of 8 bits",
            algorithmName,
            8 * size - 11
        );
    }

    failureTest(missingPublicLabel, "TypeError", () => {
        return subtle[operation](
            {name: algorithmName},
            privateKey,
            ...(isDeriveBits
                ? [8 * size]
                : [derivedKeyAlgorithm, true, derivedKeyUsages])
        );
    });

    failureTest(
        testName + " public property of algorithm is not a CryptoKey",
        "TypeError",
        () => derive(algorithmName, {message: "Not a CryptoKey"}, privateKey)
    );

    invalidPublicKeys.forEach(test => {
        failureTest(
            test.name,
            "InvalidAccessError",
            () => derive(algorithmName, test.key, privateKey)
        );
    });

    failureTest(
        testName + " no " + operation + " usage for base key",
        "InvalidAccessError",
        () => derive(algorithmName, publicKey, noUsagePrivateKey)
    );

    failureTest(
        testName + " base key is not a private key",
        "InvalidAccessError",
        () => derive(algorithmName, publicKey, publicKey)
    );

    failureTest(
        testName + " public property value is a private key",
        "InvalidAccessError",
        () => derive(algorithmName, privateKey, privateKey)
    );

    promise_test(async() => {
        const secretKey = isDeriveBits
            ? await subtle.generateKey(
                {name: "AES-CBC", length: 128},
                true,
                ["encrypt", "decrypt"]
            )
            : await subtle.generateKey(
                derivedKeyAlgorithm,
                true,
                derivedKeyUsages
            );
        const keyOptions = isDeriveBits ? {} : {
            algorithm: {name: "AES-CBC", length: 256},
            usages: ["sign", "verify"],
        };

        return derive(
            algorithmName,
            secretKey,
            privateKey,
            8 * size,
            keyOptions
        ).then(
            () => assert_unreached(
                operation + " succeeded but should have failed with InvalidAccessError"
            ),
            error => assert_equals(
                error.name,
                "InvalidAccessError",
                "Should throw correct error, not " + error.name + ": " + error.message
            )
        );
    }, testName + " public property value is a secret key");

    if (isDeriveBits) {
        failureTest(
            testName + " asking for too many bits",
            "OperationError",
            () => derive(algorithmName, publicKey, privateKey, 8 * size + 8)
        );
    }
}
