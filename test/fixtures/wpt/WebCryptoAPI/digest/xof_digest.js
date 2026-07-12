// Generates a Uint8Array of length n by repeating the pattern 00 01 02 .. F9 FA.
function ptn(n) {
  var buf = new Uint8Array(n);
  for (var i = 0; i < n; i++)
    buf[i] = i % 251;
  return buf;
}

function runXofDigestTests(subtle, options) {
  options.largeOutputTests.forEach(function (entry) {
    var alg = entry[0];
    var outputLength = entry[1];
    var lastN = entry[2];
    var expected = entry[3];

    promise_test(function (test) {
      return subtle
        .digest({ name: alg, outputLength: outputLength }, new Uint8Array(0))
        .then(function (result) {
          var full = new Uint8Array(result);
          var last = full.slice(full.length - lastN);
          assert_true(
            equalBuffers(last.buffer, hexStringToUint8Array(expected)),
            'last ' + lastN + ' bytes of digest match expected'
          );
        });
    }, alg + ' with ' + outputLength + ' bit output, verify last ' + lastN + ' bytes');
  });

  Object.keys(options.vectors).forEach(function (alg) {
    var emptyDataVector = options.vectors[alg][0];
    options.vectors[alg].forEach(function (vector, i) {
      var input = vector[0];
      var outputLength = vector[1];
      var expected = vector[2];
      var parameter = vector[3];

      var algorithmParams = { name: alg, outputLength: outputLength };
      if (parameter !== undefined)
        algorithmParams[options.parameterName] = parameter;

      var label = alg + ' vector #' + (i + 1) +
        ' (' + outputLength + ' bit output, ' + input.length + ' byte input' +
        options.formatParameter(parameter) + ')';

      promise_test(function (test) {
        return subtle
          .digest(algorithmParams, input)
          .then(function (result) {
            assert_true(
              equalBuffers(result, hexStringToUint8Array(expected)),
              'digest matches expected'
            );
          });
      }, label);

      if (input.length > 0) {
        promise_test(function (test) {
          var buffer = new Uint8Array(input);
          // Alter the buffer before calling digest
          buffer[0] = ~buffer[0];
          var duringCallParams = {
            get name() {
              // Alter the buffer back while calling digest
              buffer[0] = input[0];
              return alg;
            },
            outputLength: outputLength,
          };
          duringCallParams[options.parameterName] = parameter;
          return subtle
            .digest(duringCallParams, buffer)
            .then(function (result) {
              assert_true(
                equalBuffers(result, hexStringToUint8Array(expected)),
                'digest matches expected'
              );
            });
        }, label + ' and altered buffer during call');

        promise_test(function (test) {
          var buffer = new Uint8Array(input);
          var promise = subtle
            .digest(algorithmParams, buffer)
            .then(function (result) {
              assert_true(
                equalBuffers(result, hexStringToUint8Array(expected)),
                'digest matches expected'
              );
            });
          // Alter the buffer after calling digest
          buffer[0] = ~buffer[0];
          return promise;
        }, label + ' and altered buffer after call');

        promise_test(function (test) {
          var buffer = new Uint8Array(input);
          var duringCallParams = {
            get name() {
              // Transfer the buffer while calling digest
              buffer.buffer.transfer();
              return alg;
            },
            outputLength: outputLength,
          };
          duringCallParams[options.parameterName] = parameter;
          return subtle
            .digest(duringCallParams, buffer)
            .then(function (result) {
              if (
                options.parameterEquals(emptyDataVector, parameter) &&
                outputLength <= emptyDataVector[1]
              ) {
                assert_true(
                  equalBuffers(
                    result,
                    hexStringToUint8Array(emptyDataVector[2])
                      .subarray(0, outputLength / 8)
                  ),
                  'digest on transferred buffer should match result for empty buffer'
                );
              } else {
                assert_equals(result.byteLength, outputLength / 8,
                  'digest on transferred buffer should have correct output length');
              }
            });
        }, label + ' and transferred buffer during call');

        promise_test(function (test) {
          var buffer = new Uint8Array(input);
          var promise = subtle
            .digest(algorithmParams, buffer)
            .then(function (result) {
              assert_true(
                equalBuffers(result, hexStringToUint8Array(expected)),
                'digest matches expected'
              );
            });
          // Transfer the buffer after calling digest
          buffer.buffer.transfer();
          return promise;
        }, label + ' and transferred buffer after call');
      }
    });
  });
}
