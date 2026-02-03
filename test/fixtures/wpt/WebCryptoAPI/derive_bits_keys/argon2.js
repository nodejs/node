function define_tests() {
  var subtle = self.crypto.subtle;

  var testData = getTestData();
  var testVectors = testData.testVectors;

  return setUpBaseKeys().then(function (allKeys) {
    var baseKeys = allKeys.baseKeys;

    testVectors.forEach(function (vector) {
      var algorithmName = vector.algorithm;
      var params = vector.params;
      var expected = vector.expected;

      var testName = algorithmName + ' deriveBits';

      // Test deriveBits
      subsetTest(
        promise_test,
        function (test) {
          var algorithm = Object.assign({ name: algorithmName }, params);
          return subtle
            .deriveBits(algorithm, baseKeys[algorithmName], 256)
            .then(
              function (derivation) {
                assert_true(
                  equalBuffers(derivation, expected),
                  'Derived correct key'
                );
              },
              function (err) {
                assert_unreached(
                  'deriveBits failed with error ' +
                    err.name +
                    ': ' +
                    err.message
                );
              }
            );
        },
        testName
      );
    });
  });

  function setUpBaseKeys() {
    var promises = [];
    var baseKeys = {};

    testVectors.forEach(function (vector) {
      var algorithmName = vector.algorithm;
      var password = vector.password;

      // Key for normal operations
      promises.push(
        subtle
          .importKey('raw-secret', password, algorithmName, false, [
            'deriveBits',
          ])
          .then(function (key) {
            baseKeys[algorithmName] = key;
          })
      );
    });

    return Promise.all(promises).then(function () {
      return {
        baseKeys: baseKeys,
      };
    });
  }
}

function equalBuffers(a, b) {
  if (a.byteLength !== b.byteLength) {
    return false;
  }

  var aView = new Uint8Array(a);
  var bView = new Uint8Array(b);

  for (var i = 0; i < aView.length; i++) {
    if (aView[i] !== bView[i]) {
      return false;
    }
  }

  return true;
}
