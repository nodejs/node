function runDigestTests(subtle, sourceData, getVectors) {
  function algorithmName(algorithm) {
    return typeof algorithm === 'string' ? algorithm : algorithm.name;
  }

  function withNameGetter(algorithm, getter) {
    var result = typeof algorithm === 'string' ? {} : { ...algorithm };
    Object.defineProperty(result, 'name', {
      enumerable: true,
      get: getter,
    });
    return result;
  }

  Object.keys(sourceData).forEach(function (size) {
    getVectors(size).forEach(function (vector) {
      promise_test(function () {
        return subtle.digest(vector.algorithm, sourceData[size])
          .then(function (result) {
            assert_true(
              equalBuffers(result, vector.expected),
              'digest matches expected'
            );
          });
      }, vector.label);

      if (!vector.mutations || sourceData[size].length === 0) {
        return;
      }

      promise_test(function () {
        var buffer = new Uint8Array(sourceData[size]);
        buffer[0] = ~buffer[0];
        var algorithm = withNameGetter(vector.algorithm, function () {
          buffer[0] = sourceData[size][0];
          return algorithmName(vector.algorithm);
        });
        return subtle.digest(algorithm, buffer).then(function (result) {
          assert_true(
            equalBuffers(result, vector.expected),
            'digest matches expected'
          );
        });
      }, vector.label + ' and altered buffer during call');

      promise_test(function () {
        var buffer = new Uint8Array(sourceData[size]);
        var promise = subtle.digest(vector.algorithm, buffer)
          .then(function (result) {
            assert_true(
              equalBuffers(result, vector.expected),
              'digest matches expected'
            );
          });
        buffer[0] = ~buffer[0];
        return promise;
      }, vector.label + ' and altered buffer after call');

      promise_test(function () {
        var buffer = new Uint8Array(sourceData[size]);
        var algorithm = vector.transferBeforeCall
          ? vector.algorithm
          : withNameGetter(vector.algorithm, function () {
              buffer.buffer.transfer();
              return algorithmName(vector.algorithm);
            });
        if (vector.transferBeforeCall) {
          buffer.buffer.transfer();
        }
        return subtle.digest(algorithm, buffer).then(function (result) {
          assert_true(
            equalBuffers(result, vector.emptyExpected),
            'digest on transferred buffer should match result for empty buffer'
          );
        });
      }, vector.label + ' and transferred buffer during call');

      promise_test(function () {
        var buffer = new Uint8Array(sourceData[size]);
        var promise = subtle.digest(vector.algorithm, buffer)
          .then(function (result) {
            assert_true(
              equalBuffers(result, vector.expected),
              'digest matches expected'
            );
          });
        buffer.buffer.transfer();
        return promise;
      }, vector.label + ' and transferred buffer after call');
    });
  });
}
