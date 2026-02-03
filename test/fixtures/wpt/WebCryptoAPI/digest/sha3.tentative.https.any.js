// META: title=WebCryptoAPI: digest() SHA-3 algorithms
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
};

sourceData.long = new Uint8Array(1024 * sourceData.medium.byteLength);
for (var i = 0; i < 1024; i++) {
  sourceData.long.set(sourceData.medium, i * sourceData.medium.byteLength);
}

var digestedData = {
  'SHA3-256': {
    empty: new Uint8Array([
      167, 255, 198, 248, 191, 30, 215, 102, 81, 193, 71, 86, 160, 97, 214, 98,
      245, 128, 255, 77, 228, 59, 73, 250, 130, 216, 10, 75, 128, 248, 67, 74,
    ]),
    short: new Uint8Array([
      48, 89, 175, 122, 163, 59, 81, 112, 132, 232, 173, 123, 188, 79, 178, 8,
      164, 76, 40, 239, 50, 180, 105, 141, 16, 61, 213, 64, 228, 249, 26, 161,
    ]),
    medium: new Uint8Array([
      31, 167, 205, 29, 167, 76, 216, 4, 100, 23, 80, 140, 131, 20, 231, 74,
      154, 74, 157, 56, 249, 241, 142, 108, 178, 21, 184, 200, 145, 160, 168,
      14,
    ]),
    long: new Uint8Array([
      178, 207, 198, 30, 3, 134, 205, 174, 245, 225, 10, 43, 225, 137, 137, 31,
      94, 245, 42, 118, 36, 191, 205, 142, 220, 137, 58, 204, 100, 254, 198, 0,
    ]),
  },
  'SHA3-384': {
    empty: new Uint8Array([
      12, 99, 167, 91, 132, 94, 79, 125, 1, 16, 125, 133, 46, 76, 36, 133, 197,
      26, 80, 170, 170, 148, 252, 97, 153, 94, 113, 187, 238, 152, 58, 42, 195,
      113, 56, 49, 38, 74, 219, 71, 251, 107, 209, 224, 88, 213, 240, 4,
    ]),
    short: new Uint8Array([
      84, 184, 240, 228, 207, 73, 116, 222, 116, 0, 152, 246, 107, 48, 36, 71,
      155, 1, 99, 19, 21, 166, 119, 54, 6, 195, 62, 173, 195, 37, 86, 166, 231,
      120, 224, 143, 2, 37, 174, 121, 38, 90, 236, 102, 108, 178, 57, 11,
    ]),
    medium: new Uint8Array([
      67, 123, 125, 139, 104, 178, 80, 181, 193, 115, 158, 164, 204, 134, 219,
      32, 51, 135, 157, 251, 24, 222, 41, 44, 156, 80, 217, 193, 147, 164, 199,
      154, 8, 166, 202, 227, 244, 228, 131, 194, 121, 94, 165, 209, 239, 126,
      105, 210,
    ]),
    long: new Uint8Array([
      59, 57, 196, 201, 122, 216, 118, 19, 48, 93, 12, 204, 152, 113, 129, 113,
      62, 45, 94, 132, 177, 249, 118, 0, 17, 188, 206, 12, 41, 116, 153, 0, 91,
      220, 232, 163, 210, 64, 155, 90, 208, 22, 79, 50, 187, 135, 120, 208,
    ]),
  },
  'SHA3-512': {
    empty: new Uint8Array([
      166, 159, 115, 204, 162, 58, 154, 197, 200, 181, 103, 220, 24, 90, 117,
      110, 151, 201, 130, 22, 79, 226, 88, 89, 224, 209, 220, 193, 71, 92, 128,
      166, 21, 178, 18, 58, 241, 245, 249, 76, 17, 227, 233, 64, 44, 58, 197,
      88, 245, 0, 25, 157, 149, 182, 211, 227, 1, 117, 133, 134, 40, 29, 205,
      38,
    ]),
    short: new Uint8Array([
      45, 210, 224, 122, 98, 230, 173, 4, 152, 186, 132, 243, 19, 196, 212, 2,
      76, 180, 96, 1, 247, 143, 117, 219, 51, 107, 13, 77, 139, 210, 169, 236,
      21, 44, 74, 210, 8, 120, 115, 93, 130, 186, 8, 114, 236, 245, 150, 8, 239,
      60, 237, 43, 42, 134, 105, 66, 126, 125, 163, 30, 54, 35, 51, 216,
    ]),
    medium: new Uint8Array([
      230, 64, 162, 25, 9, 83, 102, 64, 54, 158, 155, 10, 72, 147, 28, 92, 178,
      239, 203, 201, 31, 236, 242, 71, 48, 107, 201, 106, 14, 76, 163, 51, 7,
      203, 142, 27, 154, 243, 103, 148, 109, 208, 28, 36, 63, 57, 7, 80, 141, 4,
      241, 105, 42, 49, 97, 223, 31, 137, 141, 232, 238, 37, 254, 190,
    ]),
    long: new Uint8Array([
      189, 38, 44, 236, 245, 101, 195, 56, 3, 45, 229, 186, 1, 56, 240, 170,
      207, 231, 221, 232, 61, 39, 45, 13, 55, 217, 82, 130, 158, 210, 93, 225,
      161, 52, 45, 152, 101, 158, 247, 210, 250, 74, 202, 124, 226, 177, 170, 7,
      132, 216, 252, 29, 203, 248, 27, 206, 199, 167, 67, 26, 61, 163, 107, 247,
    ]),
  },
};

// Test SHA-3 digest algorithms
Object.keys(sourceData).forEach(function (size) {
  Object.keys(digestedData).forEach(function (alg) {
    promise_test(function (test) {
      return crypto.subtle
        .digest(alg, sourceData[size])
        .then(function (result) {
          assert_true(
            equalBuffers(result, digestedData[alg][size]),
            'digest matches expected'
          );
        });
    }, alg + ' with ' + size + ' source data');

    promise_test(function (test) {
      var buffer = new Uint8Array(sourceData[size]);
      return crypto.subtle.digest(alg, buffer).then(function (result) {
        // Alter the buffer after calling digest
        if (buffer.length > 0) {
          buffer[0] = ~buffer[0];
        }
        assert_true(
          equalBuffers(result, digestedData[alg][size]),
          'digest matches expected'
        );
      });
    }, alg + ' with ' + size + ' source data and altered buffer after call');
  });
});

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
