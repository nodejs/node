
function run_test() {
  setup({explicit_done: true});

  var subtle = self.crypto.subtle; // Change to test prefixed implementations

  // When are all these tests really done? When all the promises they use have resolved.
  var all_promises = [];

  // Source file [algorithm_name]_vectors.js provides the getTestVectors method
  // for the algorithm that drives these tests.
  var testVectors = getTestVectors();

  // Test verification first, because signing tests rely on that working
  testVectors.forEach(function(vector) {
      var promise = importVectorKeys(vector, ["verify"], ["sign"])
      .then(function(vectors) {
          var algorithm = {name: vector.algorithmName};
          promise_test(function(test) {
              var operation = subtle.verify(algorithm, vector.publicKey, vector.signature, vector.data)
              .then(function(is_verified) {
                  assert_true(is_verified, "Signature verified");
              }, function(err) {
                  assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
              });

              return operation;
          }, vector.name + " verification");

      }, function(err) {
          // We need a failed test if the importVectorKey operation fails, so
          // we know we never tested verification.
          promise_test(function(test) {
              assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
          }, "importVectorKeys step: " + vector.name + " verification");
      });

      all_promises.push(promise);
  });

  // Test verification with an altered buffer after call
  testVectors.forEach(function(vector) {
      var promise = importVectorKeys(vector, ["verify"], ["sign"])
      .then(function(vectors) {
          var algorithm = {name: vector.algorithmName};
          promise_test(function(test) {
              var signature = copyBuffer(vector.signature);
              var operation = subtle.verify(algorithm, vector.publicKey, signature, vector.data)
              .then(function(is_verified) {
                  assert_true(is_verified, "Signature verified");
              }, function(err) {
                  assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
              });

              signature[0] = 255 - signature[0];
              return operation;
          }, vector.name + " verification with altered signature after call");
      }, function(err) {
          promise_test(function(test) {
              assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
          }, "importVectorKeys step: " + vector.name + " verification with altered signature after call");
      });

      all_promises.push(promise);
  });

  // Check for successful verification even if data is altered after call.
  testVectors.forEach(function(vector) {
      var promise = importVectorKeys(vector, ["verify"], ["sign"])
      .then(function(vectors) {
          var algorithm = {name: vector.algorithmName};
          promise_test(function(test) {
              var data = copyBuffer(vector.data);
              var operation = subtle.verify(algorithm, vector.publicKey, vector.signature, data)
              .then(function(is_verified) {
                  assert_true(is_verified, "Signature verified");
              }, function(err) {
                  assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
              });

              data[0] = 255 - data[0];
              return operation;
          }, vector.name + " with altered data after call");
      }, function(err) {
          promise_test(function(test) {
              assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
          }, "importVectorKeys step: " + vector.name + " with altered data after call");
      });

      all_promises.push(promise);
  });

  // Check for failures due to using privateKey to verify.
  testVectors.forEach(function(vector) {
      var promise = importVectorKeys(vector, ["verify"], ["sign"])
      .then(function(vectors) {
          var algorithm = {name: vector.algorithmName};
          promise_test(function(test) {
              return subtle.verify(algorithm, vector.privateKey, vector.signature, vector.data)
              .then(function(data) {
                  assert_unreached("Should have thrown error for using privateKey to verify in " + vector.name + ": " + err.message + "'");
              }, function(err) {
                  assert_equals(err.name, "InvalidAccessError", "Should throw InvalidAccessError instead of '" + err.message + "'");
              });
          }, vector.name + " using privateKey to verify");

      }, function(err) {
          promise_test(function(test) {
              assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
          }, "importVectorKeys step: " + vector.name + " using privateKey to verify");
      });

      all_promises.push(promise);
  });

  // Check for failures due to using publicKey to sign.
  testVectors.forEach(function(vector) {
      var promise = importVectorKeys(vector, ["verify"], ["sign"])
      .then(function(vectors) {
          var algorithm = {name: vector.algorithmName};
          promise_test(function(test) {
              return subtle.sign(algorithm, vector.publicKey, vector.data)
              .then(function(signature) {
                  assert_unreached("Should have thrown error for using publicKey to sign in " + vector.name + ": " + err.message + "'");
              }, function(err) {
                  assert_equals(err.name, "InvalidAccessError", "Should throw InvalidAccessError instead of '" + err.message + "'");
              });
          }, vector.name + " using publicKey to sign");
      }, function(err) {
          promise_test(function(test) {
              assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
          }, "importVectorKeys step: " + vector.name + " using publicKey to sign");
      });

      all_promises.push(promise);
  });

  // Check for failures due to no "verify" usage.
  testVectors.forEach(function(originalVector) {
      var vector = Object.assign({}, originalVector);

      var promise = importVectorKeys(vector, [], ["sign"])
      .then(function(vectors) {
          var algorithm = {name: vector.algorithmName};
          promise_test(function(test) {
              return subtle.verify(algorithm, vector.publicKey, vector.signature, vector.data)
              .then(function(data) {
                  assert_unreached("Should have thrown error for no verify usage in " + vector.name + ": " + err.message + "'");
              }, function(err) {
                  assert_equals(err.name, "InvalidAccessError", "Should throw InvalidAccessError instead of '" + err.message + "'");
              });
          }, vector.name + " no verify usage");
      }, function(err) {
          promise_test(function(test) {
              assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
          }, "importVectorKeys step: " + vector.name + " no verify usage");
      });

      all_promises.push(promise);
  });

  // Check for successful signing and verification.
  testVectors.forEach(function(vector) {
      var promise = importVectorKeys(vector, ["verify"], ["sign"])
      .then(function(vectors) {
          var algorithm = {name: vector.algorithmName};
          promise_test(function(test) {
              return subtle.sign(algorithm, vector.privateKey, vector.data)
              .then(function(signature) {
                  assert_true(equalBuffers(signature, vector.signature), "Signing did not give the expected output");
                  // Can we verify the signature?
                  return subtle.verify(algorithm, vector.publicKey, signature, vector.data)
                  .then(function(is_verified) {
                      assert_true(is_verified, "Round trip verification works");
                      return signature;
                  }, function(err) {
                      assert_unreached("verify error for test " + vector.name + ": " + err.message + "'");
                  });
              }, function(err) {
                  assert_unreached("sign error for test " + vector.name + ": '" + err.message + "'");
              });
          }, vector.name + " round trip");

      }, function(err) {
          // We need a failed test if the importVectorKey operation fails, so
          // we know we never tested signing or verifying
          promise_test(function(test) {
              assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
          }, "importVectorKeys step: " + vector.name + " round trip");
      });

      all_promises.push(promise);
  });

  // Test signing with the wrong algorithm
  testVectors.forEach(function(vector) {
      // Want to get the key for the wrong algorithm
      var promise = subtle.generateKey({name: "HMAC", hash: "SHA-1"}, false, ["sign", "verify"])
      .then(function(wrongKey) {
          var algorithm = {name: vector.algorithmName};
          return importVectorKeys(vector, ["verify"], ["sign"])
          .then(function(vectors) {
              promise_test(function(test) {
                  var operation = subtle.sign(algorithm, wrongKey, vector.data)
                  .then(function(signature) {
                      assert_unreached("Signing should not have succeeded for " + vector.name);
                  }, function(err) {
                      assert_equals(err.name, "InvalidAccessError", "Should have thrown InvalidAccessError instead of '" + err.message + "'");
                  });

                  return operation;
              }, vector.name + " signing with wrong algorithm name");

          }, function(err) {
              // We need a failed test if the importVectorKey operation fails, so
              // we know we never tested verification.
              promise_test(function(test) {
                  assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
              }, "importVectorKeys step: " + vector.name + " signing with wrong algorithm name");
          });
      }, function(err) {
          promise_test(function(test) {
              assert_unreached("Generate wrong key for test " + vector.name + " failed: '" + err.message + "'");
          }, "generate wrong key step: " + vector.name + " signing with wrong algorithm name");
      });

      all_promises.push(promise);
  });

  // Test verification with the wrong algorithm
  testVectors.forEach(function(vector) {
      // Want to get the key for the wrong algorithm
      var promise = subtle.generateKey({name: "HMAC", hash: "SHA-1"}, false, ["sign", "verify"])
      .then(function(wrongKey) {
          return importVectorKeys(vector, ["verify"], ["sign"])
          .then(function(vectors) {
              var algorithm = {name: vector.algorithmName};
              promise_test(function(test) {
                  var operation = subtle.verify(algorithm, wrongKey, vector.signature, vector.data)
                  .then(function(signature) {
                      assert_unreached("Verifying should not have succeeded for " + vector.name);
                  }, function(err) {
                      assert_equals(err.name, "InvalidAccessError", "Should have thrown InvalidAccessError instead of '" + err.message + "'");
                  });

                  return operation;
              }, vector.name + " verifying with wrong algorithm name");

          }, function(err) {
              // We need a failed test if the importVectorKey operation fails, so
              // we know we never tested verification.
              promise_test(function(test) {
                  assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
              }, "importVectorKeys step: " + vector.name + " verifying with wrong algorithm name");
          });
      }, function(err) {
          promise_test(function(test) {
              assert_unreached("Generate wrong key for test " + vector.name + " failed: '" + err.message + "'");
          }, "generate wrong key step: " + vector.name + " verifying with wrong algorithm name");
      });

      all_promises.push(promise);
  });

  // Test verification fails with wrong signature
  testVectors.forEach(function(vector) {
      var promise = importVectorKeys(vector, ["verify"], ["sign"])
      .then(function(vectors) {
          var algorithm = {name: vector.algorithmName};
          var signature = copyBuffer(vector.signature);
          signature[0] = 255 - signature[0];
          promise_test(function(test) {
              var operation = subtle.verify(algorithm, vector.publicKey, signature, vector.data)
              .then(function(is_verified) {
                  assert_false(is_verified, "Signature NOT verified");
              }, function(err) {
                  assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
              });

              return operation;
          }, vector.name + " verification failure due to altered signature");

      }, function(err) {
          // We need a failed test if the importVectorKey operation fails, so
          // we know we never tested verification.
          promise_test(function(test) {
              assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
          }, "importVectorKeys step: " + vector.name + " verification failure due to altered signature");
      });

      all_promises.push(promise);
  });

  // Test verification fails with short (odd length) signature
  testVectors.forEach(function(vector) {
      var promise = importVectorKeys(vector, ["verify"], ["sign"])
      .then(function(vectors) {
          var algorithm = {name: vector.algorithmName};
          var signature = vector.signature.slice(1); // Skip the first byte
          promise_test(function(test) {
              var operation = subtle.verify(algorithm, vector.publicKey, signature, vector.data)
              .then(function(is_verified) {
                  assert_false(is_verified, "Signature NOT verified");
              }, function(err) {
                  assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
              });

              return operation;
          }, vector.name + " verification failure due to shortened signature");

      }, function(err) {
          // We need a failed test if the importVectorKey operation fails, so
          // we know we never tested verification.
          promise_test(function(test) {
              assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
          }, "importVectorKeys step: " + vector.name + " verification failure due to shortened signature");
      });

      all_promises.push(promise);
  });

  // Test verification fails with wrong data
  testVectors.forEach(function(vector) {
      var promise = importVectorKeys(vector, ["verify"], ["sign"])
      .then(function(vectors) {
          var algorithm = {name: vector.algorithmName};
          var data = copyBuffer(vector.data);
          data[0] = 255 - data[0];
          promise_test(function(test) {
              var operation = subtle.verify(algorithm, vector.publicKey, vector.signature, data)
              .then(function(is_verified) {
                  assert_false(is_verified, "Signature NOT verified");
              }, function(err) {
                  assert_unreached("Verification should not throw error " + vector.name + ": " + err.message + "'");
              });

              return operation;
          }, vector.name + " verification failure due to altered data");

      }, function(err) {
          // We need a failed test if the importVectorKey operation fails, so
          // we know we never tested verification.
          promise_test(function(test) {
              assert_unreached("importVectorKeys failed for " + vector.name + ". Message: ''" + err.message + "''");
          }, "importVectorKeys step: " + vector.name + " verification failure due to altered data");
      });

      all_promises.push(promise);
  });


  promise_test(function() {
      return Promise.all(all_promises)
          .then(function() {done();})
          .catch(function() {done();})
  }, "setup");

  // Test that generated keys are valid for signing and verifying.
  testVectors.forEach(function(vector) {
      var algorithm = {name: vector.algorithmName};
      promise_test(async() => {
          let key = await subtle.generateKey(algorithm, false, ["sign", "verify"]);
          let signature = await subtle.sign(algorithm, key.privateKey, vector.data);
          let isVerified = await subtle.verify(algorithm, key.publicKey, signature, vector.data);
          assert_true(isVerified, "Verificaton failed.");
      }, "Sign and verify using generated " + vector.algorithmName + " keys.");
  });


  // A test vector has all needed fields for signing and verifying, EXCEPT that the
  // key field may be null. This function replaces that null with the Correct
  // CryptoKey object.
  //
  // Returns a Promise that yields an updated vector on success.
  function importVectorKeys(vector, publicKeyUsages, privateKeyUsages) {
      var publicPromise, privatePromise;

      if (vector.publicKey !== null) {
          publicPromise = new Promise(function(resolve, reject) {
              resolve(vector);
          });
      } else {
          publicPromise = subtle.importKey(vector.publicKeyFormat, vector.publicKeyBuffer, {name: vector.algorithmName}, false, publicKeyUsages)
          .then(function(key) {
              vector.publicKey = key;
              return vector;
          });        // Returns a copy of the sourceBuffer it is sent.
      }

      if (vector.privateKey !== null) {
          privatePromise = new Promise(function(resolve, reject) {
              resolve(vector);
          });
      } else {
          privatePromise = subtle.importKey(vector.privateKeyFormat, vector.privateKeyBuffer, {name: vector.algorithmName}, false, privateKeyUsages)
          .then(function(key) {
              vector.privateKey = key;
              return vector;
          });
      }

      return Promise.all([publicPromise, privatePromise]);
  }

  // Returns a copy of the sourceBuffer it is sent.
  function copyBuffer(sourceBuffer) {
      var source = new Uint8Array(sourceBuffer);
      var copy = new Uint8Array(sourceBuffer.byteLength)

      for (var i=0; i<source.byteLength; i++) {
          copy[i] = source[i];
      }

      return copy;
  }

  function equalBuffers(a, b) {
      if (a.byteLength !== b.byteLength) {
          return false;
      }

      var aBytes = new Uint8Array(a);
      var bBytes = new Uint8Array(b);

      for (var i=0; i<a.byteLength; i++) {
          if (aBytes[i] !== bBytes[i]) {
              return false;
          }
      }

      return true;
  }

  return;
}
