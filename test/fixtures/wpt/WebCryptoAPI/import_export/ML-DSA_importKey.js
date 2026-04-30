var subtle = crypto.subtle;

function runTests(algorithmName) {
  var algorithm = { name: algorithmName };
  var data = keyData[algorithmName];
  var jwkData = {
    jwk: { kty: data.jwk.kty, alg: data.jwk.alg, pub: data.jwk.pub },
  };

  [true, false].forEach(function (extractable) {
    // Test public keys first
    allValidUsages(data.publicUsages, true).forEach(function (usages) {
      ['spki', 'jwk', 'raw-public'].forEach(function (format) {
        if (format === 'jwk') {
          // Not all fields used for public keys
          testFormat(
            format,
            algorithm,
            jwkData,
            algorithmName,
            usages,
            extractable
          );
        } else {
          testFormat(
            format,
            algorithm,
            data,
            algorithmName,
            usages,
            extractable
          );
        }
      });
    });

    // Next, test private keys
    allValidUsages(data.privateUsages).forEach(function (usages) {
      ['pkcs8', 'jwk', 'raw-seed'].forEach(function (format) {
        testFormat(format, algorithm, data, algorithmName, usages, extractable);
      });
    });
  });
}

// Test importKey with a given key format and other parameters. If
// extrable is true, export the key and verify that it matches the input.
function testFormat(format, algorithm, keyData, keySize, usages, extractable) {
  [algorithm, algorithm.name].forEach((alg) => {
    promise_test(function (test) {
      return subtle
        .importKey(format, keyData[format], alg, extractable, usages)
        .then(
          function (key) {
            assert_equals(
              key.constructor,
              CryptoKey,
              'Imported a CryptoKey object'
            );
            assert_goodCryptoKey(
              key,
              algorithm,
              extractable,
              usages,
              format === 'pkcs8' ||
                format === 'raw-seed' ||
                (format === 'jwk' && keyData[format].priv)
                ? 'private'
                : 'public'
            );
            if (!extractable) {
              return;
            }

            return subtle.exportKey(format, key).then(
              function (result) {
                if (format !== 'jwk') {
                  assert_true(
                    equalBuffers(keyData[format], result),
                    'Round trip works'
                  );
                } else {
                  assert_true(
                    equalJwk(keyData[format], result),
                    'Round trip works'
                  );
                }
              },
              function (err) {
                assert_unreached(
                  'Threw an unexpected error: ' + err.toString()
                );
              }
            );
          },
          function (err) {
            assert_unreached('Threw an unexpected error: ' + err.toString());
          }
        );
    }, 'Good parameters: ' +
      keySize.toString() +
      ' bits ' +
      parameterString(format, keyData[format], alg, extractable, usages));
  });
}

// Helper methods follow:

// Convert method parameters to a string to uniquely name each test
function parameterString(format, data, algorithm, extractable, usages) {
  if ('byteLength' in data) {
    data = 'buffer(' + data.byteLength.toString() + ')';
  } else {
    data = 'object(' + Object.keys(data).join(', ') + ')';
  }
  var result =
    '(' +
    objectToString(format) +
    ', ' +
    objectToString(data) +
    ', ' +
    objectToString(algorithm) +
    ', ' +
    objectToString(extractable) +
    ', ' +
    objectToString(usages) +
    ')';

  return result;
}
