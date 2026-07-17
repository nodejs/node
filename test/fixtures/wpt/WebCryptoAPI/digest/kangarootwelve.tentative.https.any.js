// META: title=WebCryptoAPI: digest() KangarooTwelve algorithms
// META: script=../util/helpers.js
// META: timeout=long

var subtle = crypto.subtle; // Change to test prefixed implementations

// Generates a Uint8Array of length n by repeating the pattern 00 01 02 .. F9 FA.
function ptn(n) {
  var buf = new Uint8Array(n);
  for (var i = 0; i < n; i++)
    buf[i] = i % 251;
  return buf;
}

function hexToBytes(hex) {
  var bytes = new Uint8Array(hex.length / 2);
  for (var i = 0; i < hex.length; i += 2)
    bytes[i / 2] = parseInt(hex.substring(i, i + 2), 16);
  return bytes;
}

// RFC 9861 Section 5 test vectors
// [input, outputLengthBits, expected hex(, customization)]
var kt128Vectors = [
  [new Uint8Array(0), 256,
   '1ac2d450fc3b4205d19da7bfca1b3751' +
   '3c0803577ac7167f06fe2ce1f0ef39e5'],
  [new Uint8Array(0), 512,
   '1ac2d450fc3b4205d19da7bfca1b3751' +
   '3c0803577ac7167f06fe2ce1f0ef39e5' +
   '4269c056b8c82e48276038b6d292966c' +
   'c07a3d4645272e31ff38508139eb0a71'],
  [ptn(1), 256,
   '2bda92450e8b147f8a7cb629e784a058' +
   'efca7cf7d8218e02d345dfaa65244a1f'],
  [ptn(17), 256,
   '6bf75fa2239198db4772e36478f8e19b' +
   '0f371205f6a9a93a273f51df37122888'],
  [ptn(Math.pow(17, 2)), 256,
   '0c315ebcdedbf61426de7dcf8fb725d1' +
   'e74675d7f5327a5067f367b108ecb67c'],
  [ptn(Math.pow(17, 3)), 256,
   'cb552e2ec77d9910701d578b457ddf77' +
   '2c12e322e4ee7fe417f92c758f0d59d0'],
  [ptn(Math.pow(17, 4)), 256,
   '8701045e22205345ff4dda05555cbb5c' +
   '3af1a771c2b89baef37db43d9998b9fe'],
  [ptn(Math.pow(17, 5)), 256,
   '844d610933b1b9963cbdeb5ae3b6b05c' +
   'c7cbd67ceedf883eb678a0a8e0371682'],
  [ptn(Math.pow(17, 6)), 256,
   '3c390782a8a4e89fa6367f72feaaf132' +
   '55c8d95878481d3cd8ce85f58e880af8'],
  [new Uint8Array(0), 256,
   'fab658db63e94a246188bf7af69a1330' +
   '45f46ee984c56e3c3328caaf1aa1a583', ptn(1)],
  [new Uint8Array([0xff]), 256,
   'd848c5068ced736f4462159b9867fd4c' +
   '20b808acc3d5bc48e0b06ba0a3762ec4', ptn(41)],
  [new Uint8Array([0xff, 0xff, 0xff]), 256,
   'c389e5009ae57120854c2e8c64670ac0' +
   '1358cf4c1baf89447a724234dc7ced74', ptn(Math.pow(41, 2))],
  [new Uint8Array([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]), 256,
   '75d2f86a2e644566726b4fbcfc5657b9' +
   'dbcf070c7b0dca06450ab291d7443bcf', ptn(Math.pow(41, 3))],
  [ptn(8191), 256,
   '1b577636f723643e990cc7d6a6598374' +
   '36fd6a103626600eb8301cd1dbe553d6'],
  [ptn(8192), 256,
   '48f256f6772f9edfb6a8b661ec92dc93' +
   'b95ebd05a08a17b39ae3490870c926c3'],
  [ptn(8192), 256,
   '3ed12f70fb05ddb58689510ab3e4d23c' +
   '6c6033849aa01e1d8c220a297fedcd0b', ptn(8189)],
  [ptn(8192), 256,
   '6a7c1b6a5cd0d8c9ca943a4a216cc646' +
   '04559a2ea45f78570a15253d67ba00ae', ptn(8190)],
];

var kt256Vectors = [
  [new Uint8Array(0), 512,
   'b23d2e9cea9f4904e02bec06817fc10c' +
   'e38ce8e93ef4c89e6537076af8646404' +
   'e3e8b68107b8833a5d30490aa3348235' +
   '3fd4adc7148ecb782855003aaebde4a9'],
  [new Uint8Array(0), 1024,
   'b23d2e9cea9f4904e02bec06817fc10c' +
   'e38ce8e93ef4c89e6537076af8646404' +
   'e3e8b68107b8833a5d30490aa3348235' +
   '3fd4adc7148ecb782855003aaebde4a9' +
   'b0925319d8ea1e121a609821ec19efea' +
   '89e6d08daee1662b69c840289f188ba8' +
   '60f55760b61f82114c030c97e5178449' +
   '608ccd2cd2d919fc7829ff69931ac4d0'],
  [ptn(1), 512,
   '0d005a194085360217128cf17f91e1f7' +
   '1314efa5564539d444912e3437efa17f' +
   '82db6f6ffe76e781eaa068bce01f2bbf' +
   '81eacb983d7230f2fb02834a21b1ddd0'],
  [ptn(17), 512,
   '1ba3c02b1fc514474f06c8979978a905' +
   '6c8483f4a1b63d0dccefe3a28a2f323e' +
   '1cdcca40ebf006ac76ef039715234683' +
   '7b1277d3e7faa9c9653b19075098527b'],
  [ptn(Math.pow(17, 2)), 512,
   'de8ccbc63e0f133ebb4416814d4c66f6' +
   '91bbf8b6a61ec0a7700f836b086cb029' +
   'd54f12ac7159472c72db118c35b4e6aa' +
   '213c6562caaa9dcc518959e69b10f3ba'],
  [ptn(Math.pow(17, 3)), 512,
   '647efb49fe9d717500171b41e7f11bd4' +
   '91544443209997ce1c2530d15eb1ffbb' +
   '598935ef954528ffc152b1e4d731ee26' +
   '83680674365cd191d562bae753b84aa5'],
  [ptn(Math.pow(17, 4)), 512,
   'b06275d284cd1cf205bcbe57dccd3ec1' +
   'ff6686e3ed15776383e1f2fa3c6ac8f0' +
   '8bf8a162829db1a44b2a43ff83dd89c3' +
   'cf1ceb61ede659766d5ccf817a62ba8d'],
  [ptn(Math.pow(17, 5)), 512,
   '9473831d76a4c7bf77ace45b59f1458b' +
   '1673d64bcd877a7c66b2664aa6dd149e' +
   '60eab71b5c2bab858c074ded81ddce2b' +
   '4022b5215935c0d4d19bf511aeeb0772'],
  [ptn(Math.pow(17, 6)), 512,
   '0652b740d78c5e1f7c8dcc1777097382' +
   '768b7ff38f9a7a20f29f413bb1b3045b' +
   '31a5578f568f911e09cf44746da84224' +
   'a5266e96a4a535e871324e4f9c7004da'],
  [new Uint8Array(0), 512,
   '9280f5cc39b54a5a594ec63de0bb9937' +
   '1e4609d44bf845c2f5b8c316d72b1598' +
   '11f748f23e3fabbe5c3226ec96c62186' +
   'df2d33e9df74c5069ceecbb4dd10eff6', ptn(1)],
  [new Uint8Array([0xff]), 512,
   '47ef96dd616f200937aa7847e34ec2fe' +
   'ae8087e3761dc0f8c1a154f51dc9ccf8' +
   '45d7adbce57ff64b639722c6a1672e3b' +
   'f5372d87e00aff89be97240756998853', ptn(41)],
  [new Uint8Array([0xff, 0xff, 0xff]), 512,
   '3b48667a5051c5966c53c5d42b95de45' +
   '1e05584e7806e2fb765eda959074172c' +
   'b438a9e91dde337c98e9c41bed94c4e0' +
   'aef431d0b64ef2324f7932caa6f54969', ptn(Math.pow(41, 2))],
  [new Uint8Array([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]), 512,
   'e0911cc00025e1540831e266d94add9b' +
   '98712142b80d2629e643aac4efaf5a3a' +
   '30a88cbf4ac2a91a2432743054fbcc98' +
   '97670e86ba8cec2fc2ace9c966369724', ptn(Math.pow(41, 3))],
  [ptn(8191), 512,
   '3081434d93a4108d8d8a3305b89682ce' +
   'bedc7ca4ea8a3ce869fbb73cbe4a58ee' +
   'f6f24de38ffc170514c70e7ab2d01f03' +
   '812616e863d769afb3753193ba045b20'],
  [ptn(8192), 512,
   'c6ee8e2ad3200c018ac87aaa031cdac2' +
   '2121b412d07dc6e0dccbb53423747e9a' +
   '1c18834d99df596cf0cf4b8dfafb7bf0' +
   '2d139d0c9035725adc1a01b7230a41fa'],
  [ptn(8192), 512,
   '74e47879f10a9c5d11bd2da7e194fe57' +
   'e86378bf3c3f7448eff3c576a0f18c5c' +
   'aae0999979512090a7f348af4260d4de' +
   '3c37f1ecaf8d2c2c96c1d16c64b12496', ptn(8189)],
  [ptn(8192), 512,
   'f4b5908b929ffe01e0f79ec2f21243d4' +
   '1a396b2e7303a6af1d6399cd6c7a0a2d' +
   'd7c4f607e8277f9c9b1cb4ab9ddc59d4' +
   'b92d1fc7558441f1832c3279a4241b8b', ptn(8190)],
];

// Large output tests: verify last N bytes of extended output
var largeOutputTests = [
  // [algorithm, outputLengthBits, lastNBytes, expectedLastBytes]
  ['KT128', 10032 * 8, 32,
   'e8dc563642f7228c84684c898405d3a8' +
   '34799158c079b12880277a1d28e2ff6d'],
  ['KT256', 10064 * 8, 64,
   'ad4a1d718cf950506709a4c33396139b' +
   '4449041fc79a05d68da35f1e453522e0' +
   '56c64fe94958e7085f2964888259b993' +
   '2752f3ccd855288efee5fcbb8b563069'],
];

largeOutputTests.forEach(function (entry) {
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
          equalBuffers(last.buffer, hexToBytes(expected)),
          'last ' + lastN + ' bytes of digest match expected'
        );
      });
  }, alg + ' with ' + outputLength + ' bit output, verify last ' + lastN + ' bytes');
});

function customizationEqual(emptyDataVector, customization) {
  return equalBuffers(customization ?? new Uint8Array(0), emptyDataVector[3] ?? new Uint8Array(0));
}

function outputLengthLessOrEqual(emptyDataVector, outputLength) {
  return outputLength <= emptyDataVector[1];
}

var allVectors = {
  KT128: kt128Vectors,
  KT256: kt256Vectors,
};

Object.keys(allVectors).forEach(function (alg) {
  var emptyDataVector = allVectors[alg][0];
  allVectors[alg].forEach(function (vector, i) {
    var input = vector[0];
    var outputLength = vector[1];
    var expected = vector[2];
    var customization = vector[3];

    var algorithmParams = { name: alg, outputLength: outputLength };
    if (customization !== undefined)
      algorithmParams.customization = customization;

    var label = alg + ' vector #' + (i + 1) +
      ' (' + outputLength + ' bit output, ' + input.length + ' byte input' +
      (customization !== undefined ? ', C=' + customization.length + ' bytes' : '') + ')';

    promise_test(function (test) {
      return subtle
        .digest(algorithmParams, input)
        .then(function (result) {
          assert_true(
            equalBuffers(result, hexToBytes(expected)),
            'digest matches expected'
          );
        });
    }, label);

    if (input.length > 0) {
      promise_test(function (test) {
        var buffer = new Uint8Array(input);
        // Alter the buffer before calling digest
        buffer[0] = ~buffer[0];
        return subtle
          .digest({
            get name() {
              // Alter the buffer back while calling digest
              buffer[0] = input[0];
              return alg;
            },
            outputLength: outputLength,
            customization: customization,
          }, buffer)
          .then(function (result) {
            assert_true(
              equalBuffers(result, hexToBytes(expected)),
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
              equalBuffers(result, hexToBytes(expected)),
              'digest matches expected'
            );
          });
        // Alter the buffer after calling digest
        buffer[0] = ~buffer[0];
        return promise;
      }, label + ' and altered buffer after call');

      promise_test(function (test) {
        var buffer = new Uint8Array(input);
        return subtle
          .digest({
            get name() {
              // Transfer the buffer while calling digest
              buffer.buffer.transfer();
              return alg;
            },
            outputLength: outputLength,
            customization: customization,
          }, buffer)
          .then(function (result) {
            if (customizationEqual(emptyDataVector, customization) && outputLengthLessOrEqual(emptyDataVector, outputLength)) {
              assert_true(
                equalBuffers(result, Uint8Array.fromHex(emptyDataVector[2]).subarray(0, outputLength / 8)),
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
              equalBuffers(result, hexToBytes(expected)),
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
