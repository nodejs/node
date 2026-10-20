function run_test() {
    const subtle = self.crypto.subtle;
    const testVectors = getTestVectors().map(
        vector => ({...vector, data: vector.plaintext})
    );
    const importAlgorithm = vector => ({
        name: vector.algorithm.name,
        hash: vector.hash,
    });

    runSignatureTests({
        vectors: testVectors,
        algorithmIdentifier(vector) {
            return vector.algorithm;
        },
        importAlgorithm,
        dataLabel: "plaintext",
        shortSignature: false,
        wrongVerifyLabel: " verification with wrong algorithm name",
        alteredSignatureLabel: " verification failure with altered signature",
        alteredDataLabel: " verification failure with altered plaintext",
        async wrongKey(vector, operation) {
            const name = vector.algorithm.name === "RSA-PSS"
                ? "RSASSA-PKCS1-v1_5"
                : "RSA-PSS";
            const isSign = operation === "sign";
            return subtle.importKey(
                isSign ? vector.privateKeyFormat : vector.publicKeyFormat,
                isSign ? vector.privateKeyBuffer : vector.publicKeyBuffer,
                {name, hash: vector.hash},
                false,
                [operation]
            );
        },
        async roundTrip({
            vector,
            algorithm,
            verificationKey,
            signingKey,
        }) {
            const isDeterministic = !("saltLength" in algorithm) ||
                algorithm.saltLength === 0;
            const signature = await subtle.sign(
                algorithm,
                signingKey,
                vector.data
            );

            if (isDeterministic) {
                assert_true(
                    equalBuffers(signature, vector.signature),
                    "Signing did not give the expected output"
                );
            }

            const isVerified = await subtle.verify(
                algorithm,
                verificationKey,
                signature,
                vector.data
            );
            assert_true(isVerified, "Round trip verifies");

            const secondSignature = await subtle.sign(
                algorithm,
                signingKey,
                vector.data
            );
            if (isDeterministic) {
                assert_true(
                    equalBuffers(signature, secondSignature),
                    "Two signings with empty salt give same signature"
                );
            } else {
                assert_false(
                    equalBuffers(signature, secondSignature),
                    "Two signings with a salt give different signatures"
                );
            }
        },
    });

    testVectors.forEach(function(vector) {
        if (vector.algorithm.name !== "RSA-PSS") {
            return;
        }

        promise_test(async function() {
            const key = await subtle.importKey(
                vector.publicKeyFormat,
                vector.publicKeyBuffer,
                importAlgorithm(vector),
                false,
                ["verify"]
            );
            const saltLength = vector.algorithm.saltLength === 32 ? 48 : 32;
            const isVerified = await subtle.verify(
                {...vector.algorithm, saltLength},
                key,
                vector.signature,
                vector.data
            );
            assert_false(isVerified, "Signature NOT verified");
        }, vector.name + " verification failure with wrong saltLength");
    });
}
