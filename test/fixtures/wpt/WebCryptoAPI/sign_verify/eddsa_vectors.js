// eddsa_vectors.js

// Data for testing Ed25519 and Ed448.

// The following function returns an array of test vectors
// for the subtleCrypto sign method.
//
// Each test vector has the following fields:
//     name - a unique name for this vector
//     publicKeyBuffer - an arrayBuffer with the key data
//     publicKeyFormat - "spki" "jwk"
//     publicKey - a CryptoKey object for the keyBuffer. INITIALLY null! You must fill this in first to use it!
//     privateKeyBuffer - an arrayBuffer with the key data
//     privateKeyFormat - "pkcs8" or "jwk"
//     privateKey - a CryptoKey object for the keyBuffer. INITIALLY null! You must fill this in first to use it!
//     algorithmName - the name of the AlgorithmIdentifier parameter to provide to sign
//     data - the text to sign
//     signature - the expected signature
function getTestVectors() {
  var pkcs8 = {
      "Ed25519": new Uint8Array([48, 46, 2, 1, 0, 48, 5, 6, 3, 43, 101, 112, 4, 34, 4, 32, 243, 200, 244, 196, 141, 248, 120, 20, 110, 140, 211, 191, 109, 244, 229, 14, 56, 155, 167, 7, 78, 21, 194, 53, 45, 205, 93, 48, 141, 76, 168, 31]),
      "Ed448": new Uint8Array([48, 71, 2, 1, 0, 48, 5, 6, 3, 43, 101, 113, 4, 59, 4, 57, 14, 255, 3, 69, 140, 40, 224, 23, 156, 82, 29, 227, 18, 201, 105, 183, 131, 67, 72, 236, 171, 153, 26, 96, 227, 178, 233, 167, 158, 76, 217, 228, 128, 239, 41, 23, 18, 210, 200, 61, 4, 114, 114, 213, 201, 244, 40, 102, 79, 105, 109, 38, 112, 69, 143, 29, 46]),
  };

  var spki = {
      "Ed25519": new Uint8Array([48, 42, 48, 5, 6, 3, 43, 101, 112, 3, 33, 0, 216, 225, 137, 99, 216, 9, 212, 135, 217, 84, 154, 204, 174, 198, 116, 46, 126, 235, 162, 77, 138, 13, 59, 20, 183, 227, 202, 234, 6, 137, 61, 204]),
      "Ed448": new Uint8Array([48, 67, 48, 5, 6, 3, 43, 101, 113, 3, 58, 0, 171, 75, 184, 133, 253, 125, 44, 90, 242, 78, 131, 113, 12, 255, 160, 199, 74, 87, 226, 116, 128, 29, 178, 5, 123, 11, 220, 94, 160, 50, 182, 254, 107, 199, 139, 128, 69, 54, 90, 235, 38, 232, 110, 31, 20, 253, 52, 157, 7, 196, 132, 149, 245, 164, 106, 90, 128]),
  };

  // data
  var data = new Uint8Array([43, 126, 208, 188, 119, 149, 105, 74, 180, 172, 211, 89, 3, 254, 140, 215, 216, 15, 106, 28, 134, 136, 166, 195, 65, 68, 9, 69, 117, 20, 161, 69, 120, 85, 187, 178, 25, 227, 10, 27, 238, 168, 254, 134, 144, 130, 217, 159, 200, 40, 47, 144, 80, 208, 36, 229, 158, 175, 7, 48, 186, 157, 183, 10]);

  // For verification tests.
  var signatures = {
      "Ed25519": new Uint8Array([61, 144, 222, 94, 87, 67, 223, 194, 130, 37, 191, 173, 179, 65, 177, 22, 203, 248, 163, 241, 206, 237, 191, 74, 220, 53, 14, 245, 211, 71, 24, 67, 164, 24, 97, 77, 203, 110, 97, 72, 98, 97, 76, 247, 175, 20, 150, 249, 52, 11, 60, 132, 78, 164, 220, 234, 177, 211, 209, 85, 235, 126, 204, 0]),
      "Ed448": new Uint8Array([118, 137, 126, 140, 80, 172, 107, 17, 50, 115, 92, 9, 197, 95, 80, 108, 1, 73, 210, 103, 124, 117, 102, 79, 139, 193, 11, 130, 111, 189, 157, 240, 160, 60, 217, 134, 188, 232, 51, 158, 100, 199, 209, 114, 14, 169, 54, 23, 132, 220, 115, 131, 119, 101, 172, 41, 128, 192, 218, 192, 129, 74, 139, 193, 135, 209, 201, 201, 7, 197, 220, 192, 121, 86, 248, 91, 112, 147, 15, 228, 45, 231, 100, 23, 114, 23, 203, 45, 82, 186, 183, 193, 222, 190, 12, 168, 156, 206, 203, 205, 99, 247, 2, 90, 42, 90, 87, 43, 157, 35, 176, 100, 47, 0]),
  }

  var vectors = [];
  ["Ed25519", "Ed448"].forEach(function(algorithmName) {
    var vector = {
      name: "EdDSA " + algorithmName,
      publicKeyBuffer: spki[algorithmName],
      publicKeyFormat: "spki",
      publicKey: null,
      privateKeyBuffer: pkcs8[algorithmName],
      privateKeyFormat: "pkcs8",
      privateKey: null,
      algorithmName: algorithmName,
      data: data,
      signature: signatures[algorithmName]
    };

    vectors.push(vector);
  });

  return vectors;
}
