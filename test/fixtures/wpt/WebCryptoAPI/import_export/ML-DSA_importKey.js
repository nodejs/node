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

// Are two array buffers the same?
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

// Are two Jwk objects "the same"? That is, does the object returned include
// matching values for each property that was expected? It's okay if the
// returned object has extra methods; they aren't checked.
function equalJwk(expected, got) {
  var fields = Object.keys(expected);
  var fieldName;

  for (var i = 0; i < fields.length; i++) {
    fieldName = fields[i];
    if (!(fieldName in got)) {
      return false;
    }
    if (expected[fieldName] !== got[fieldName]) {
      return false;
    }
  }

  return true;
}

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

// Character representation of any object we may use as a parameter.
function objectToString(obj) {
  var keyValuePairs = [];

  if (Array.isArray(obj)) {
    return (
      '[' +
      obj
        .map(function (elem) {
          return objectToString(elem);
        })
        .join(', ') +
      ']'
    );
  } else if (typeof obj === 'object') {
    Object.keys(obj)
      .sort()
      .forEach(function (keyName) {
        keyValuePairs.push(keyName + ': ' + objectToString(obj[keyName]));
      });
    return '{' + keyValuePairs.join(', ') + '}';
  } else if (typeof obj === 'undefined') {
    return 'undefined';
  } else {
    return obj.toString();
  }

  var keyValuePairs = [];

  Object.keys(obj)
    .sort()
    .forEach(function (keyName) {
      var value = obj[keyName];
      if (typeof value === 'object') {
        value = objectToString(value);
      } else if (typeof value === 'array') {
        value =
          '[' +
          value
            .map(function (elem) {
              return objectToString(elem);
            })
            .join(', ') +
          ']';
      } else {
        value = value.toString();
      }

      keyValuePairs.push(keyName + ': ' + value);
    });

  return '{' + keyValuePairs.join(', ') + '}';
}
