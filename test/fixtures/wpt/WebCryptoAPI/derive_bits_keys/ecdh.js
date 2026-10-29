async function defineEcdhTests(operation) {
    const subtle = self.crypto.subtle;
    const fixtures = getEcdhTestFixtures();
    const curves = Object.keys(fixtures.sizes);
    const keys = {};

    await Promise.all(curves.map(async namedCurve => {
        const algorithm = {name: "ECDH", namedCurve};
        const noUsage = operation === "deriveBits" ? ["deriveKey"] : ["deriveBits"];
        const [
            privateKey,
            noUsagePrivateKey,
            publicKey,
            ecdsaKeyPair,
        ] = await Promise.all([
            subtle.importKey(
                "pkcs8",
                fixtures.pkcs8[namedCurve],
                algorithm,
                false,
                ["deriveBits", "deriveKey"]
            ),
            subtle.importKey(
                "pkcs8",
                fixtures.pkcs8[namedCurve],
                algorithm,
                false,
                noUsage
            ),
            subtle.importKey(
                "spki",
                fixtures.spki[namedCurve],
                algorithm,
                false,
                []
            ),
            subtle.generateKey(
                {name: "ECDSA", namedCurve},
                false,
                ["sign", "verify"]
            ),
        ]);

        keys[namedCurve] = {
            privateKey,
            noUsagePrivateKey,
            publicKey,
            ecdsaKeyPair,
        };
    }));

    curves.forEach(namedCurve => {
        const otherCurve = namedCurve === "P-256" ? "P-384" : "P-256";
        const key = keys[namedCurve];
        registerDeriveTests({
            operation,
            algorithmName: "ECDH",
            testName: namedCurve,
            mixedCaseName: "EcDh",
            size: fixtures.sizes[namedCurve],
            derivation: fixtures.derivations[namedCurve],
            privateKey: key.privateKey,
            publicKey: key.publicKey,
            noUsagePrivateKey: key.noUsagePrivateKey,
            invalidPublicKeys: [
                {
                    name: namedCurve + " mismatched curves",
                    key: keys[otherCurve].publicKey,
                    length: 256,
                },
                {
                    name: namedCurve +
                        " public property of algorithm is not an ECDSA public key",
                    key: key.ecdsaKeyPair.publicKey,
                },
            ],
            missingPublicLabel: namedCurve + " missing public curve",
        });
    });
}
