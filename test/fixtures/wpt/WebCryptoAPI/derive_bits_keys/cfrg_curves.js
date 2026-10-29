async function defineCfrgTests(algorithmName, operation) {
    const subtle = self.crypto.subtle;
    const isDeriveBits = operation === "deriveBits";

    kSmallOrderPoint[algorithmName].forEach(test => {
        promise_test(async() => {
            let privateKey;
            let publicKey;
            let derived;
            let error;

            try {
                privateKey = await subtle.importKey(
                    "pkcs8",
                    pkcs8[algorithmName],
                    {name: algorithmName},
                    false,
                    ["deriveBits", "deriveKey"]
                );
                publicKey = await subtle.importKey(
                    "spki",
                    test.vector,
                    {name: algorithmName},
                    false,
                    []
                );
                derived = isDeriveBits
                    ? await subtle.deriveBits(
                        {name: algorithmName, public: publicKey},
                        privateKey,
                        8 * sizes[algorithmName]
                    )
                    : await subtle.deriveKey(
                        {name: algorithmName, public: publicKey},
                        privateKey,
                        {name: "HMAC", hash: "SHA-256", length: 256},
                        true,
                        ["sign", "verify"]
                    );
            } catch (caught) {
                error = caught;
            }

            assert_not_equals(privateKey, undefined, "Private key should be valid.");
            assert_not_equals(publicKey, undefined, "Public key should be valid.");
            assert_not_equals(error, undefined, "Operation should fail.");
            assert_equals(
                error.name,
                "OperationError",
                "Should throw correct error, not " + error.name + ": " + error.message + "."
            );
            assert_equals(derived, undefined, "Operation succeeded, but should not have.");
        }, algorithmName +
            (isDeriveBits ? " key derivation" : " deriveBits") +
            " checks for all-zero value result with a key of order " + test.order);
    });

    if (!isDeriveBits) {
        promise_test(async() => {
            const key = await subtle.generateKey(
                {name: algorithmName},
                true,
                ["deriveKey", "deriveBits"]
            );
            const derived = await subtle.deriveKey(
                {name: algorithmName, public: key.publicKey},
                key.privateKey,
                {name: "HMAC", hash: "SHA-256", length: 256},
                true,
                ["sign", "verify"]
            );
            assert_not_equals(derived, undefined, "Key derivation failed.");
        }, "Key derivation using a " + algorithmName + " generated keys.");
    }

    const noUsage = isDeriveBits ? ["deriveKey"] : ["deriveBits"];
    const [
        privateKey,
        noUsagePrivateKey,
        publicKey,
        ecdhPublicKey,
    ] = await Promise.all([
        subtle.importKey(
            "pkcs8",
            pkcs8[algorithmName],
            {name: algorithmName},
            false,
            ["deriveBits", "deriveKey"]
        ),
        subtle.importKey(
            "pkcs8",
            pkcs8[algorithmName],
            {name: algorithmName},
            false,
            noUsage
        ),
        subtle.importKey(
            "spki",
            spki[algorithmName],
            {name: algorithmName},
            false,
            []
        ),
        subtle.importKey(
            "spki",
            ecSPKI,
            {name: "ECDH", namedCurve: "P-256"},
            false,
            []
        ),
    ]);

    registerDeriveTests({
        operation,
        algorithmName,
        mixedCaseName: algorithmName.toLowerCase(),
        size: sizes[algorithmName],
        derivation: derivations[algorithmName],
        privateKey,
        publicKey,
        noUsagePrivateKey,
        invalidPublicKeys: [{
            name: algorithmName + " mismatched algorithms",
            key: ecdhPublicKey,
        }],
        missingPublicLabel: algorithmName + " missing public property",
    });
}
