// aes_gcm_vectors.js

// The following function returns an array of test vectors
// for the subtleCrypto encrypt method.
//
// Each test vector has the following fields:
//     name - a unique name for this vector
//     keyBuffer - an arrayBuffer with the key data in raw form
//     key - a CryptoKey object for the keyBuffer. INITIALLY null! You must fill this in first to use it!
//     algorithm - the value of the AlgorithmIdentifier parameter to provide to encrypt
//     plaintext - the text to encrypt
//     result - the expected result (usually just ciphertext, sometimes with added authentication)
function getTestVectors() {
    const {
      plaintext,
      keyBytes,
      iv,
      additionalData,
      tag,
      tag_with_empty_ad,
      ciphertext,
    } = getFixtures();

    var keyLengths = [128, 192, 256];
    var tagLengths = [32, 64, 96, 104, 112, 120, 128];

    // All the scenarios that should succeed, if the key has "encrypt" usage
    var passing = [];
    keyLengths.forEach(function(keyLength) {
        tagLengths.forEach(function(tagLength) {
            var byteCount = tagLength / 8;

            var result = new Uint8Array(ciphertext[keyLength].byteLength + byteCount);
            result.set(ciphertext[keyLength], 0);
            result.set(tag[keyLength].slice(0, byteCount), ciphertext[keyLength].byteLength);
            passing.push({
                    name: "AES-GCM " + keyLength.toString() + "-bit key, " + tagLength.toString() + "-bit tag, " + (iv.byteLength << 3).toString() + "-bit iv",
                    keyBuffer: keyBytes[keyLength],
                    key: null,
                    algorithm: {name: "AES-GCM", iv: iv, additionalData: additionalData, tagLength: tagLength},
                    plaintext: plaintext,
                    result: result
            });

            var noadresult = new Uint8Array(ciphertext[keyLength].byteLength + byteCount);
            noadresult.set(ciphertext[keyLength], 0);
            noadresult.set(tag_with_empty_ad[keyLength].slice(0, byteCount), ciphertext[keyLength].byteLength);
            passing.push({
                    name: "AES-GCM " + keyLength.toString() + "-bit key, no additional data, " + tagLength.toString() + "-bit tag, " + (iv.byteLength << 3).toString() + "-bit iv",
                    keyBuffer: keyBytes[keyLength],
                    key: null,
                    algorithm: {name: "AES-GCM", iv: iv, tagLength: tagLength},
                    plaintext: plaintext,
                    result: noadresult
            });
        });
    });

    // Scenarios that should fail because of a bad tag length, causing an OperationError
    var failing = [];
    keyLengths.forEach(function(keyLength) {
        // First, make some tests for bad tag lengths
        [24, 48, 72, 95, 129].forEach(function(badTagLength) {
            failing.push({
                name: "AES-GCM " + keyLength.toString() + "-bit key, " + (iv.byteLength << 3).toString() + "-bit iv, " + "illegal tag length " + badTagLength.toString() + "-bits",
                keyBuffer: keyBytes[keyLength],
                key: null,
                algorithm: {name: "AES-GCM", iv: iv, additionalData: additionalData, tagLength: badTagLength},
                plaintext: plaintext,
                result: ciphertext[keyLength]
            });
        });
    });

    return {passing: passing, failing: failing, decryptionFailing: []};
}
