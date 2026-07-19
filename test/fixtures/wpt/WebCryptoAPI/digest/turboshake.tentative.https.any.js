// META: title=WebCryptoAPI: digest() TurboSHAKE algorithms
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
// [input, outputLengthBits, expected hex(, domainSeparation)]
var turboSHAKE128Vectors = [
  [new Uint8Array(0), 256,
   '1e415f1c5983aff2169217277d17bb53' +
   '8cd945a397ddec541f1ce41af2c1b74c'],
  [new Uint8Array(0), 512,
   '1e415f1c5983aff2169217277d17bb53' +
   '8cd945a397ddec541f1ce41af2c1b74c' +
   '3e8ccae2a4dae56c84a04c2385c03c15' +
   'e8193bdf58737363321691c05462c8df'],
  [ptn(1), 256,
   '55cedd6f60af7bb29a4042ae832ef3f5' +
   '8db7299f893ebb9247247d856958daa9'],
  [ptn(17), 256,
   '9c97d036a3bac819db70ede0ca554ec6' +
   'e4c2a1a4ffbfd9ec269ca6a111161233'],
  [ptn(Math.pow(17, 2)), 256,
   '96c77c279e0126f7fc07c9b07f5cdae1' +
   'e0be60bdbe10620040e75d7223a624d2'],
  [ptn(Math.pow(17, 3)), 256,
   'd4976eb56bcf118520582b709f73e1d6' +
   '853e001fdaf80e1b13e0d0599d5fb372'],
  [ptn(Math.pow(17, 4)), 256,
   'da67c7039e98bf530cf7a37830c6664e' +
   '14cbab7f540f58403b1b82951318ee5c'],
  [ptn(Math.pow(17, 5)), 256,
   'b97a906fbf83ef7c812517abf3b2d0ae' +
   'a0c4f60318ce11cf103925127f59eecd'],
  [ptn(Math.pow(17, 6)), 256,
   '35cd494adeded2f25239af09a7b8ef0c' +
   '4d1ca4fe2d1ac370fa63216fe7b4c2b1'],
  [new Uint8Array([0xff, 0xff, 0xff]), 256,
   'bf323f940494e88ee1c540fe660be8a0' +
   'c93f43d15ec006998462fa994eed5dab', 0x01],
  [new Uint8Array([0xff]), 256,
   '8ec9c66465ed0d4a6c35d13506718d68' +
   '7a25cb05c74cca1e42501abd83874a67', 0x06],
  [new Uint8Array([0xff, 0xff, 0xff]), 256,
   'b658576001cad9b1e5f399a9f77723bb' +
   'a05458042d68206f7252682dba3663ed', 0x07],
  [new Uint8Array([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]), 256,
   '8deeaa1aec47ccee569f659c21dfa8e1' +
   '12db3cee37b18178b2acd805b799cc37', 0x0b],
  [new Uint8Array([0xff]), 256,
   '553122e2135e363c3292bed2c6421fa2' +
   '32bab03daa07c7d6636603286506325b', 0x30],
  [new Uint8Array([0xff, 0xff, 0xff]), 256,
   '16274cc656d44cefd422395d0f9053bd' +
   'a6d28e122aba15c765e5ad0e6eaf26f9', 0x7f],
];

var turboSHAKE256Vectors = [
  [new Uint8Array(0), 512,
   '367a329dafea871c7802ec67f905ae13' +
   'c57695dc2c6663c61035f59a18f8e7db' +
   '11edc0e12e91ea60eb6b32df06dd7f00' +
   '2fbafabb6e13ec1cc20d995547600db0'],
  [ptn(1), 512,
   '3e1712f928f8eaf1054632b2aa0a246e' +
   'd8b0c378728f60bc970410155c28820e' +
   '90cc90d8a3006aa2372c5c5ea176b068' +
   '2bf22bae7467ac94f74d43d39b0482e2'],
  [ptn(17), 512,
   'b3bab0300e6a191fbe61379398359235' +
   '78794ea54843f5011090fa2f3780a9e5' +
   'cb22c59d78b40a0fbff9e672c0fbe097' +
   '0bd2c845091c6044d687054da5d8e9c7'],
  [ptn(Math.pow(17, 2)), 512,
   '66b810db8e90780424c0847372fdc957' +
   '10882fde31c6df75beb9d4cd9305cfca' +
   'e35e7b83e8b7e6eb4b78605880116316' +
   'fe2c078a09b94ad7b8213c0a738b65c0'],
  [ptn(Math.pow(17, 3)), 512,
   'c74ebc919a5b3b0dd1228185ba02d29e' +
   'f442d69d3d4276a93efe0bf9a16a7dc0' +
   'cd4eabadab8cd7a5edd96695f5d360ab' +
   'e09e2c6511a3ec397da3b76b9e1674fb'],
  [ptn(Math.pow(17, 4)), 512,
   '02cc3a8897e6f4f6ccb6fd46631b1f52' +
   '07b66c6de9c7b55b2d1a23134a170afd' +
   'ac234eaba9a77cff88c1f020b7372461' +
   '8c5687b362c430b248cd38647f848a1d'],
  [ptn(Math.pow(17, 5)), 512,
   'add53b06543e584b5823f626996aee50' +
   'fe45ed15f20243a7165485acb4aa76b4' +
   'ffda75cedf6d8cdc95c332bd56f4b986' +
   'b58bb17d1778bfc1b1a97545cdf4ec9f'],
  [ptn(Math.pow(17, 6)), 512,
   '9e11bc59c24e73993c1484ec66358ef7' +
   '1db74aefd84e123f7800ba9c4853e02c' +
   'fe701d9e6bb765a304f0dc34a4ee3ba8' +
   '2c410f0da70e86bfbd90ea877c2d6104'],
  [new Uint8Array([0xff, 0xff, 0xff]), 512,
   'd21c6fbbf587fa2282f29aea620175fb' +
   '0257413af78a0b1b2a87419ce031d933' +
   'ae7a4d383327a8a17641a34f8a1d1003' +
   'ad7da6b72dba84bb62fef28f62f12424', 0x01],
  [new Uint8Array([0xff]), 512,
   '738d7b4e37d18b7f22ad1b5313e357e3' +
   'dd7d07056a26a303c433fa3533455280' +
   'f4f5a7d4f700efb437fe6d281405e07b' +
   'e32a0a972e22e63adc1b090daefe004b', 0x06],
  [new Uint8Array([0xff, 0xff, 0xff]), 512,
   '18b3b5b7061c2e67c1753a00e6ad7ed7' +
   'ba1c906cf93efb7092eaf27fbeebb755' +
   'ae6e292493c110e48d260028492b8e09' +
   'b5500612b8f2578985ded5357d00ec67', 0x07],
  [new Uint8Array([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]), 512,
   'bb36764951ec97e9d85f7ee9a67a7718' +
   'fc005cf42556be79ce12c0bde50e5736' +
   'd6632b0d0dfb202d1bbb8ffe3dd74cb0' +
   '0834fa756cb03471bab13a1e2c16b3c0', 0x0b],
  [new Uint8Array([0xff]), 512,
   'f3fe12873d34bcbb2e608779d6b70e7f' +
   '86bec7e90bf113cbd4fdd0c4e2f4625e' +
   '148dd7ee1a52776cf77f240514d9ccfc' +
   '3b5ddab8ee255e39ee389072962c111a', 0x30],
  [new Uint8Array([0xff, 0xff, 0xff]), 512,
   'abe569c1f77ec340f02705e7d37c9ab7' +
   'e155516e4a6a150021d70b6fac0bb40c' +
   '069f9a9828a0d575cd99f9bae435ab1a' +
   'cf7ed9110ba97ce0388d074bac768776', 0x7f],
];

// Large output tests: verify last 32 bytes of extended output
var largeOutputTests = [
  // [algorithm, outputLengthBits, lastNBytes, expectedLastBytes]
  ['TurboSHAKE128', 10032 * 8, 32,
   'a3b9b0385900ce761f22aed548e754da' +
   '10a5242d62e8c658e3f3a923a7555607'],
  ['TurboSHAKE256', 10032 * 8, 32,
   'abefa11630c661269249742685ec082f' +
   '207265dccf2f43534e9c61ba0c9d1d75'],
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

function domainSeparationEqual(emptyDataVector, domainSeparation) {
  return (domainSeparation ?? 0x1f) === (emptyDataVector[3] ?? 0x1f);
}

function outputLengthLessOrEqual(emptyDataVector, outputLength) {
  return outputLength <= emptyDataVector[1];
}

var allVectors = {
  TurboSHAKE128: turboSHAKE128Vectors,
  TurboSHAKE256: turboSHAKE256Vectors,
};

Object.keys(allVectors).forEach(function (alg) {
  var emptyDataVector = allVectors[alg][0];
  allVectors[alg].forEach(function (vector, i) {
    var input = vector[0];
    var outputLength = vector[1];
    var expected = vector[2];
    var domainSeparation = vector[3];

    var algorithmParams = { name: alg, outputLength: outputLength };
    if (domainSeparation !== undefined)
      algorithmParams.domainSeparation = domainSeparation;

    var label = alg + ' vector #' + (i + 1) +
      ' (' + outputLength + ' bit output, ' + input.length + ' byte input' +
      (domainSeparation !== undefined ? ', D=0x' + domainSeparation.toString(16) : '') + ')';

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
            domainSeparation: domainSeparation,
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
            domainSeparation: domainSeparation,
          }, buffer)
          .then(function (result) {
            if (domainSeparationEqual(emptyDataVector, domainSeparation) && outputLengthLessOrEqual(emptyDataVector, outputLength)) {
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
