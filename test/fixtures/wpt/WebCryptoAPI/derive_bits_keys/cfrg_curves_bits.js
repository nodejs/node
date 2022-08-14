
function define_tests() {
  // May want to test prefixed implementations.
  var subtle = self.crypto.subtle;

  var pkcs8 = {
      "X25519": new Uint8Array([48, 46, 2, 1, 0, 48, 5, 6, 3, 43, 101, 110, 4, 34, 4, 32, 200, 131, 142, 118, 208, 87, 223, 183, 216, 201, 90, 105, 225, 56, 22, 10, 221, 99, 115, 253, 113, 164, 210, 118, 187, 86, 227, 168, 27, 100, 255, 97]),
      "X448": new Uint8Array([48, 70, 2, 1, 0, 48, 5, 6, 3, 43, 101, 111, 4, 58, 4, 56, 88, 199, 210, 154, 62, 181, 25, 178, 157, 0, 207, 177, 145, 187, 100, 252, 109, 138, 66, 216, 241, 113, 118, 39, 43, 137, 242, 39, 45, 24, 25, 41, 92, 101, 37, 192, 130, 150, 113, 176, 82, 239, 7, 39, 83, 15, 24, 142, 49, 208, 204, 83, 191, 38, 146, 158])
  };

  var spki = {
      "X25519": new Uint8Array([48, 42, 48, 5, 6, 3, 43, 101, 110, 3, 33, 0, 28, 242, 177, 230, 2, 46, 197, 55, 55, 30, 215, 245, 62, 84, 250, 17, 84, 216, 62, 152, 235, 100, 234, 81, 250, 229, 179, 48, 124, 254, 151, 6]),
      "X448": new Uint8Array([48, 66, 48, 5, 6, 3, 43, 101, 111, 3, 57, 0, 182, 4, 161, 209, 165, 205, 29, 148, 38, 213, 97, 239, 99, 10, 158, 177, 108, 190, 105, 213, 185, 202, 97, 94, 220, 83, 99, 62, 251, 82, 234, 49, 230, 230, 160, 161, 219, 172, 198, 231, 108, 188, 230, 72, 45, 126, 75, 163, 213, 93, 158, 128, 39, 101, 206, 111])
  };

  var sizes = {
      "X25519": 32,
      "X448": 56
  };

  var derivations = {
      "X25519": new Uint8Array([39, 104, 64, 157, 250, 185, 158, 194, 59, 140, 137, 185, 63, 245, 136, 2, 149, 247, 97, 118, 8, 143, 137, 228, 61, 254, 190, 126, 161, 149, 0, 8]),
      "X448": new Uint8Array([240, 246, 197, 241, 127, 148, 244, 41, 30, 171, 113, 120, 134, 109, 55, 236, 137, 6, 221, 108, 81, 65, 67, 220, 133, 190, 124, 242, 141, 239, 243, 155, 114, 110, 15, 109, 207, 129, 14, 181, 148, 220, 169, 123, 72, 130, 189, 68, 196, 62, 167, 220, 103, 244, 154, 78])
  };

  return importKeys(pkcs8, spki, sizes)
  .then(function(results) {
      publicKeys = results.publicKeys;
      privateKeys = results.privateKeys;
      noDeriveBitsKeys = results.noDeriveBitsKeys;

      Object.keys(sizes).forEach(function(algorithmName) {
          // Basic success case
          promise_test(function(test) {
              return subtle.deriveBits({name: algorithmName, public: publicKeys[algorithmName]}, privateKeys[algorithmName], 8 * sizes[algorithmName])
              .then(function(derivation) {
                  assert_true(equalBuffers(derivation, derivations[algorithmName]), "Derived correct bits");
              }, function(err) {
                  assert_unreached("deriveBits failed with error " + err.name + ": " + err.message);
              });
          }, algorithmName + " good parameters");

          // Case insensitivity check
          promise_test(function(test) {
              return subtle.deriveBits({name: algorithmName.toLowerCase(), public: publicKeys[algorithmName]}, privateKeys[algorithmName], 8 * sizes[algorithmName])
              .then(function(derivation) {
                  assert_true(equalBuffers(derivation, derivations[algorithmName]), "Derived correct bits");
              }, function(err) {
                  assert_unreached("deriveBits failed with error " + err.name + ": " + err.message);
              });
          }, algorithmName + " mixed case parameters");

          // Null length
          promise_test(function(test) {
              return subtle.deriveBits({name: algorithmName, public: publicKeys[algorithmName]}, privateKeys[algorithmName], null)
              .then(function(derivation) {
                  assert_true(equalBuffers(derivation, derivations[algorithmName]), "Derived correct bits");
              }, function(err) {
                  assert_unreached("deriveBits failed with error " + err.name + ": " + err.message);
              });
          }, algorithmName + " with null length");

          // Shorter than entire derivation per algorithm
          promise_test(function(test) {
              return subtle.deriveBits({name: algorithmName, public: publicKeys[algorithmName]}, privateKeys[algorithmName], 8 * sizes[algorithmName] - 32)
              .then(function(derivation) {
                  assert_true(equalBuffers(derivation, derivations[algorithmName], 8 * sizes[algorithmName] - 32), "Derived correct bits");
              }, function(err) {
                  assert_unreached("deriveBits failed with error " + err.name + ": " + err.message);
              });
          }, algorithmName + " short result");

          // Non-multiple of 8
          promise_test(function(test) {
              return subtle.deriveBits({name: algorithmName, public: publicKeys[algorithmName]}, privateKeys[algorithmName], 8 * sizes[algorithmName] - 11)
              .then(function(derivation) {
                  assert_true(equalBuffers(derivation, derivations[algorithmName], 8 * sizes[algorithmName] - 11), "Derived correct bits");
              }, function(err) {
                  assert_unreached("deriveBits failed with error " + err.name + ": " + err.message);
              });
          }, algorithmName + " non-multiple of 8 bits");

          // Errors to test:

          // - missing public property TypeError
          promise_test(function(test) {
              return subtle.deriveBits({name: algorithmName}, privateKeys[algorithmName], 8 * sizes[algorithmName])
              .then(function(derivation) {
                  assert_unreached("deriveBits succeeded but should have failed with TypeError");
              }, function(err) {
                  assert_equals(err.name, "TypeError", "Should throw correct error, not " + err.name + ": " + err.message);
              });
          }, algorithmName + " missing public property");

          // - Non CryptoKey public property TypeError
          promise_test(function(test) {
              return subtle.deriveBits({name: algorithmName, public: {message: "Not a CryptoKey"}}, privateKeys[algorithmName], 8 * sizes[algorithmName])
              .then(function(derivation) {
                  assert_unreached("deriveBits succeeded but should have failed with TypeError");
              }, function(err) {
                  assert_equals(err.name, "TypeError", "Should throw correct error, not " + err.name + ": " + err.message);
              });
          }, algorithmName + " public property of algorithm is not a CryptoKey");

          // - wrong algorithm
          promise_test(function(test) {
              publicKey = publicKeys["X25519"];
              if (algorithmName === "X25519") {
                  publicKey = publicKeys["X448"];
              }
              return subtle.deriveBits({name: algorithmName, public: publicKey}, privateKeys[algorithmName], 8 * sizes[algorithmName])
              .then(function(derivation) {
                  assert_unreached("deriveBits succeeded but should have failed with InvalidAccessError");
              }, function(err) {
                  assert_equals(err.name, "InvalidAccessError", "Should throw correct error, not " + err.name + ": " + err.message);
              });
          }, algorithmName + " mismatched algorithms");

          // - No deriveBits usage in baseKey InvalidAccessError
          promise_test(function(test) {
              return subtle.deriveBits({name: algorithmName, public: publicKeys[algorithmName]}, noDeriveBitsKeys[algorithmName], 8 * sizes[algorithmName])
              .then(function(derivation) {
                  assert_unreached("deriveBits succeeded but should have failed with InvalidAccessError");
              }, function(err) {
                  assert_equals(err.name, "InvalidAccessError", "Should throw correct error, not " + err.name + ": " + err.message);
              });
          }, algorithmName + " no deriveBits usage for base key");

          // - Use public key for baseKey InvalidAccessError
          promise_test(function(test) {
              return subtle.deriveBits({name: algorithmName, public: publicKeys[algorithmName]}, publicKeys[algorithmName], 8 * sizes[algorithmName])
              .then(function(derivation) {
                  assert_unreached("deriveBits succeeded but should have failed with InvalidAccessError");
              }, function(err) {
                  assert_equals(err.name, "InvalidAccessError", "Should throw correct error, not " + err.name + ": " + err.message);
              });
          }, algorithmName + " base key is not a private key");

          // - Use private key for public property InvalidAccessError
          promise_test(function(test) {
              return subtle.deriveBits({name: algorithmName, public: privateKeys[algorithmName]}, privateKeys[algorithmName], 8 * sizes[algorithmName])
              .then(function(derivation) {
                  assert_unreached("deriveBits succeeded but should have failed with InvalidAccessError");
              }, function(err) {
                  assert_equals(err.name, "InvalidAccessError", "Should throw correct error, not " + err.name + ": " + err.message);
              });
          }, algorithmName + " public property value is a private key");

          // - Use secret key for public property InvalidAccessError
          promise_test(function(test) {
              return subtle.generateKey({name: "AES-CBC", length: 128}, true, ["encrypt", "decrypt"])
              .then(function(secretKey) {
                  subtle.deriveBits({name: algorithmName, public: secretKey}, privateKeys[algorithmName], 8 * sizes[algorithmName])
                  .then(function(derivation) {
                      assert_unreached("deriveBits succeeded but should have failed with InvalidAccessError");
                  }, function(err) {
                      assert_equals(err.name, "InvalidAccessError", "Should throw correct error, not " + err.name + ": " + err.message);
                  });
              });
          }, algorithmName + " public property value is a secret key");

          // - Length greater than possible for particular curves OperationError
          promise_test(function(test) {
              return subtle.deriveBits({name: algorithmName, public: publicKeys[algorithmName]}, privateKeys[algorithmName], 8 * sizes[algorithmName] + 8)
              .then(function(derivation) {
                  assert_unreached("deriveBits succeeded but should have failed with OperationError");
              }, function(err) {
                  assert_equals(err.name, "OperationError", "Should throw correct error, not " + err.name + ": " + err.message);
              });
          }, algorithmName + " asking for too many bits");
      });
  });

  function importKeys(pkcs8, spki, sizes) {
      var privateKeys = {};
      var publicKeys = {};
      var noDeriveBitsKeys = {};

      var promises = [];
      Object.keys(pkcs8).forEach(function(algorithmName) {
          var operation = subtle.importKey("pkcs8", pkcs8[algorithmName],
                                          {name: algorithmName},
                                          false, ["deriveBits", "deriveKey"])
                          .then(function(key) {
                              privateKeys[algorithmName] = key;
                          });
          promises.push(operation);
      });
      Object.keys(pkcs8).forEach(function(algorithmName) {
          var operation = subtle.importKey("pkcs8", pkcs8[algorithmName],
                                          {name: algorithmName},
                                          false, ["deriveKey"])
                          .then(function(key) {
                              noDeriveBitsKeys[algorithmName] = key;
                          });
          promises.push(operation);
      });
      Object.keys(spki).forEach(function(algorithmName) {
          var operation = subtle.importKey("spki", spki[algorithmName],
                                          {name: algorithmName},
                                          false, [])
                          .then(function(key) {
                              publicKeys[algorithmName] = key;
                          });
          promises.push(operation);
      });

      return Promise.all(promises)
             .then(function(results) {return {privateKeys: privateKeys, publicKeys: publicKeys, noDeriveBitsKeys: noDeriveBitsKeys}});
  }

  // Compares two ArrayBuffer or ArrayBufferView objects. If bitCount is
  // omitted, the two values must be the same length and have the same contents
  // in every byte. If bitCount is included, only that leading number of bits
  // have to match.
  function equalBuffers(a, b, bitCount) {
      var remainder;

      if (typeof bitCount === "undefined" && a.byteLength !== b.byteLength) {
          return false;
      }

      var aBytes = new Uint8Array(a);
      var bBytes = new Uint8Array(b);

      var length = a.byteLength;
      if (typeof bitCount !== "undefined") {
          length = Math.floor(bitCount / 8);
      }

      for (var i=0; i<length; i++) {
          if (aBytes[i] !== bBytes[i]) {
              return false;
          }
      }

      if (typeof bitCount !== "undefined") {
          remainder = bitCount % 8;
          return aBytes[length] >> (8 - remainder) === bBytes[length] >> (8 - remainder);
      }

      return true;
  }

}
