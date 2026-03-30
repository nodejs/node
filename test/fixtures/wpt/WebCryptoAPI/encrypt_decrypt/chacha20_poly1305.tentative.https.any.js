// META: title=WebCryptoAPI: encrypt()/decrypt() ChaCha20-Poly1305
// META: timeout=long

var subtle = crypto.subtle; // Change to test prefixed implementations

var sourceData = {
  empty: new Uint8Array(0),
  short: new Uint8Array([
    21, 110, 234, 124, 193, 76, 86, 203, 148, 219, 3, 10, 74, 157, 149, 255,
  ]),
  medium: new Uint8Array([
    182, 200, 249, 223, 100, 140, 208, 136, 183, 15, 56, 231, 65, 151, 177, 140,
    184, 30, 30, 67, 80, 213, 11, 204, 184, 251, 90, 115, 121, 200, 123, 178,
    227, 214, 237, 84, 97, 237, 30, 159, 54, 243, 64, 163, 150, 42, 68, 107,
    129, 91, 121, 75, 75, 212, 58, 68, 3, 80, 32, 119, 178, 37, 108, 200, 7,
    131, 127, 58, 172, 209, 24, 235, 75, 156, 43, 174, 184, 151, 6, 134, 37,
    171, 172, 161, 147,
  ]),
  long: new Uint8Array(
    Array(65)
      .fill(0)
      .map((_, i) => i % 256)
  ),
};

var additionalData = {
  empty: new Uint8Array(0),
  short: new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]),
  medium: new Uint8Array([
    9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
  ]),
};

var iv = new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]); // 96-bit IV (only valid size for ChaCha20-Poly1305)

var keyBytes = new Uint8Array([
  52, 138, 105, 103, 105, 3, 94, 254, 59, 241, 159, 138, 189, 254, 153, 191,
  228, 172, 165, 239, 117, 172, 19, 206, 219, 9, 205, 138, 45, 87, 166, 89,
]);

var encryptedData = {
  empty_ad: {
    empty: new Uint8Array([
      202, 31, 160, 169, 169, 221, 162, 53, 252, 126, 127, 237, 158, 98, 86, 41,
    ]),
    short: new Uint8Array([
      226, 238, 222, 234, 202, 96, 116, 202, 205, 199, 126, 40, 167, 93, 65,
      172, 87, 45, 146, 68, 245, 236, 87, 162, 200, 89, 228, 51, 2, 249, 103,
      255,
    ]),
    medium: new Uint8Array([
      65, 72, 205, 73, 111, 160, 242, 137, 238, 19, 69, 197, 172, 87, 101, 223,
      97, 245, 255, 57, 226, 43, 36, 125, 205, 218, 211, 252, 102, 45, 234, 179,
      23, 71, 87, 156, 145, 16, 106, 98, 75, 140, 67, 249, 230, 124, 37, 127,
      192, 145, 40, 129, 203, 31, 15, 182, 15, 226, 211, 111, 132, 132, 114,
      204, 73, 36, 245, 199, 212, 187, 239, 33, 46, 203, 124, 134, 76, 21, 242,
      233, 119, 179, 216, 193, 210, 145, 43, 48, 105, 153, 28, 62, 49, 175, 154,
      142, 222, 68, 219, 229, 66,
    ]),
    long: new Uint8Array([
      247, 129, 54, 149, 15, 41, 36, 6, 81, 21, 119, 41, 225, 205, 218, 92, 201,
      250, 243, 105, 166, 235, 57, 166, 109, 56, 147, 148, 3, 248, 143, 30, 212,
      176, 152, 235, 212, 216, 82, 218, 85, 86, 41, 113, 92, 123, 79, 59, 113,
      251, 99, 249, 180, 254, 3, 197, 52, 139, 201, 35, 10, 156, 32, 59, 14,
      225, 117, 48, 100, 126, 58, 112, 157, 195, 18, 185, 178, 97, 215, 233,
      113,
    ]),
  },
  short_ad: {
    empty: new Uint8Array([
      0, 176, 175, 37, 156, 39, 141, 93, 198, 224, 223, 214, 239, 212, 50, 215,
    ]),
    short: new Uint8Array([
      226, 238, 222, 234, 202, 96, 116, 202, 205, 199, 126, 40, 167, 93, 65,
      172, 40, 45, 199, 67, 109, 80, 99, 203, 246, 137, 82, 206, 183, 23, 175,
      176,
    ]),
    medium: new Uint8Array([
      65, 72, 205, 73, 111, 160, 242, 137, 238, 19, 69, 197, 172, 87, 101, 223,
      97, 245, 255, 57, 226, 43, 36, 125, 205, 218, 211, 252, 102, 45, 234, 179,
      23, 71, 87, 156, 145, 16, 106, 98, 75, 140, 67, 249, 230, 124, 37, 127,
      192, 145, 40, 129, 203, 31, 15, 182, 15, 226, 211, 111, 132, 132, 114,
      204, 73, 36, 245, 199, 212, 187, 239, 33, 46, 203, 124, 134, 76, 21, 242,
      233, 119, 179, 216, 193, 210, 199, 121, 7, 153, 245, 137, 122, 100, 72,
      102, 113, 169, 244, 218, 111, 105,
    ]),
    long: new Uint8Array([
      247, 129, 54, 149, 15, 41, 36, 6, 81, 21, 119, 41, 225, 205, 218, 92, 201,
      250, 243, 105, 166, 235, 57, 166, 109, 56, 147, 148, 3, 248, 143, 30, 212,
      176, 152, 235, 212, 216, 82, 218, 85, 86, 41, 113, 92, 123, 79, 59, 113,
      251, 99, 249, 180, 254, 3, 197, 52, 139, 201, 35, 10, 156, 32, 59, 14,
      137, 0, 55, 193, 166, 74, 124, 251, 91, 36, 191, 112, 236, 204, 35, 102,
    ]),
  },
  medium_ad: {
    empty: new Uint8Array([
      163, 122, 139, 166, 205, 190, 132, 236, 17, 19, 31, 4, 16, 47, 13, 242,
    ]),
    short: new Uint8Array([
      226, 238, 222, 234, 202, 96, 116, 202, 205, 199, 126, 40, 167, 93, 65,
      172, 18, 242, 253, 220, 52, 138, 67, 162, 201, 38, 125, 119, 44, 53, 147,
      119,
    ]),
    medium: new Uint8Array([
      65, 72, 205, 73, 111, 160, 242, 137, 238, 19, 69, 197, 172, 87, 101, 223,
      97, 245, 255, 57, 226, 43, 36, 125, 205, 218, 211, 252, 102, 45, 234, 179,
      23, 71, 87, 156, 145, 16, 106, 98, 75, 140, 67, 249, 230, 124, 37, 127,
      192, 145, 40, 129, 203, 31, 15, 182, 15, 226, 211, 111, 132, 132, 114,
      204, 73, 36, 245, 199, 212, 187, 239, 33, 46, 203, 124, 134, 76, 21, 242,
      233, 119, 179, 216, 193, 210, 161, 250, 192, 51, 193, 133, 185, 55, 148,
      21, 194, 168, 212, 203, 252, 184,
    ]),
    long: new Uint8Array([
      247, 129, 54, 149, 15, 41, 36, 6, 81, 21, 119, 41, 225, 205, 218, 92, 201,
      250, 243, 105, 166, 235, 57, 166, 109, 56, 147, 148, 3, 248, 143, 30, 212,
      176, 152, 235, 212, 216, 82, 218, 85, 86, 41, 113, 92, 123, 79, 59, 113,
      251, 99, 249, 180, 254, 3, 197, 52, 139, 201, 35, 10, 156, 32, 59, 14, 88,
      20, 159, 185, 145, 230, 1, 242, 106, 255, 55, 7, 60, 85, 179, 184,
    ]),
  },
};

function equalBuffers(a, b) {
  if (a.byteLength !== b.byteLength) {
    return false;
  }
  var aBytes = new Uint8Array(a);
  var bBytes = new Uint8Array(b);
  for (var i = 0; i < a.byteLength; i++) {
    if (aBytes[i] !== bBytes[i]) {
      return false;
    }
  }
  return true;
}

// Test ChaCha20-Poly1305 encryption/decryption
var algorithmName = 'ChaCha20-Poly1305';

// Test with different additional data combinations
Object.keys(additionalData).forEach(function (adName) {
  var ad = additionalData[adName];

  Object.keys(sourceData).forEach(function (dataName) {
    var plaintext = sourceData[dataName];
    var ciphertext = encryptedData[adName + '_ad'][dataName];

    promise_test(function (test) {
      var operation = subtle
        .importKey('raw-secret', keyBytes, { name: algorithmName }, false, [
          'encrypt',
          'decrypt',
        ])
        .then(
          function (key) {
            return subtle.encrypt(
              { name: algorithmName, iv: iv, additionalData: ad },
              key,
              plaintext
            );
          },
          function (err) {
            assert_unreached(
              'importKey failed unexpectedly: ' + err.toString()
            );
          }
        )
        .then(
          function (result) {
            assert_true(
              equalBuffers(result, ciphertext),
              'Encrypted data should match expected result'
            );
            return result;
          },
          function (err) {
            assert_unreached('encrypt failed unexpectedly: ' + err.toString());
          }
        );

      return operation;
    }, algorithmName +
      ' encryption with ' +
      adName +
      ' additional data, ' +
      dataName +
      ' data');

    promise_test(function (test) {
      var operation = subtle
        .importKey('raw-secret', keyBytes, { name: algorithmName }, false, [
          'encrypt',
          'decrypt',
        ])
        .then(
          function (key) {
            return subtle.decrypt(
              { name: algorithmName, iv: iv, additionalData: ad },
              key,
              ciphertext
            );
          },
          function (err) {
            assert_unreached(
              'importKey failed unexpectedly: ' + err.toString()
            );
          }
        )
        .then(
          function (result) {
            assert_true(
              equalBuffers(result, plaintext),
              'Decrypted data should match original plaintext'
            );
          },
          function (err) {
            assert_unreached('decrypt failed unexpectedly: ' + err.toString());
          }
        );

      return operation;
    }, algorithmName +
      ' decryption with ' +
      adName +
      ' additional data, ' +
      dataName +
      ' data');
  });
});

// Test round-trip encryption/decryption without fixed vectors
promise_test(function (test) {
  var key;
  var plaintext = sourceData.medium;
  var ad = additionalData.short;

  return subtle
    .generateKey({ name: algorithmName }, false, ['encrypt', 'decrypt'])
    .then(function (result) {
      key = result;
      return subtle.encrypt(
        { name: algorithmName, iv: iv, additionalData: ad },
        key,
        plaintext
      );
    })
    .then(function (encrypted) {
      return subtle.decrypt(
        { name: algorithmName, iv: iv, additionalData: ad },
        key,
        encrypted
      );
    })
    .then(function (decrypted) {
      assert_true(
        equalBuffers(decrypted, plaintext),
        'Round-trip encryption/decryption should return original plaintext'
      );
    });
}, algorithmName + ' round-trip encrypt/decrypt');

// Test decryption with wrong additional data should fail
promise_test(function (test) {
  var key;
  var plaintext = sourceData.short;
  var correctAd = additionalData.short;
  var wrongAd = additionalData.medium;

  return subtle
    .generateKey({ name: algorithmName }, false, ['encrypt', 'decrypt'])
    .then(function (result) {
      key = result;
      return subtle.encrypt(
        { name: algorithmName, iv: iv, additionalData: correctAd },
        key,
        plaintext
      );
    })
    .then(function (encrypted) {
      return promise_rejects_dom(
        test,
        'OperationError',
        subtle.decrypt(
          { name: algorithmName, iv: iv, additionalData: wrongAd },
          key,
          encrypted
        )
      );
    });
}, algorithmName + ' decryption with wrong additional data should fail');

// Test decryption with wrong IV should fail
promise_test(function (test) {
  var key;
  var correctIv = iv;
  var wrongIv = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]); // Different 96-bit IV
  var plaintext = sourceData.short;
  var ad = additionalData.empty;

  return subtle
    .generateKey({ name: algorithmName }, false, ['encrypt', 'decrypt'])
    .then(function (result) {
      key = result;
      return subtle.encrypt(
        { name: algorithmName, iv: correctIv, additionalData: ad },
        key,
        plaintext
      );
    })
    .then(function (encrypted) {
      return promise_rejects_dom(
        test,
        'OperationError',
        subtle.decrypt(
          { name: algorithmName, iv: wrongIv, additionalData: ad },
          key,
          encrypted
        )
      );
    });
}, algorithmName + ' decryption with wrong IV should fail');

// Test invalid IV size should fail
promise_test(function (test) {
  var invalidIv = new Uint8Array(16); // 128-bit IV is invalid for ChaCha20-Poly1305

  return subtle
    .generateKey({ name: algorithmName }, false, ['encrypt'])
    .then(function (key) {
      return promise_rejects_dom(
        test,
        'OperationError',
        subtle.encrypt(
          { name: algorithmName, iv: invalidIv },
          key,
          sourceData.short
        )
      );
    });
}, algorithmName + ' encryption with invalid IV size should fail');
