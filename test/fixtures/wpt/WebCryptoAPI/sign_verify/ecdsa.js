function run_test() {
    const subtle = self.crypto.subtle;
    const normalize = vector => ({...vector, data: vector.plaintext});
    const testVectors = getTestVectors().map(normalize);
    const invalidTestVectors = getInvalidTestVectors().map(normalize);
    const algorithmIdentifier = vector => ({
        name: vector.algorithmName,
        hash: vector.hashName,
    });
    const importAlgorithm = vector => ({
        name: vector.algorithmName,
        namedCurve: vector.namedCurve,
    });

    runSignatureTests({
        vectors: testVectors,
        invalidVectors: invalidTestVectors,
        algorithmIdentifier,
        importAlgorithm,
        dataLabel: "plaintext",
    });

    testVectors.forEach(function(vector) {
        promise_test(async function() {
            const key = await subtle.importKey(
                vector.publicKeyFormat,
                vector.publicKeyBuffer,
                importAlgorithm(vector),
                false,
                ["verify"]
            );
            const hash = vector.hashName === "SHA-1" ? "SHA-256" : "SHA-1";
            const isVerified = await subtle.verify(
                {name: vector.algorithmName, hash},
                key,
                vector.signature,
                vector.data
            );
            assert_false(isVerified, "Signature NOT verified");
        }, vector.name + " verification failure due to wrong hash");

        promise_test(async function() {
            const key = await subtle.importKey(
                vector.publicKeyFormat,
                vector.publicKeyBuffer,
                importAlgorithm(vector),
                false,
                ["verify"]
            );
            const hash = vector.hashName.substring(0, 3) +
                vector.hashName.substring(4);
            let error;
            try {
                await subtle.verify(
                    {name: vector.algorithmName, hash},
                    key,
                    vector.signature,
                    vector.data
                );
            } catch (caught) {
                error = caught;
            }
            assert_not_equals(error, undefined, "Verification should throw");
            assert_equals(
                error.name,
                "NotSupportedError",
                "Correctly throws NotSupportedError for illegal hash name"
            );
        }, vector.name + " verification failure due to bad hash name");
    });
}
