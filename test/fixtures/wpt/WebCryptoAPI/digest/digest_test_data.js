function getDigestSourceData(includeLong) {
  var sourceData = {
    empty: new Uint8Array(0),
    short: new Uint8Array([
      21, 110, 234, 124, 193, 76, 86, 203, 148, 219, 3, 10, 74, 157, 149, 255,
    ]),
    medium: new Uint8Array([
      182, 200, 249, 223, 100, 140, 208, 136, 183, 15, 56, 231, 65, 151, 177,
      140, 184, 30, 30, 67, 80, 213, 11, 204, 184, 251, 90, 115, 121, 200, 123,
      178, 227, 214, 237, 84, 97, 237, 30, 159, 54, 243, 64, 163, 150, 42, 68,
      107, 129, 91, 121, 75, 75, 212, 58, 68, 3, 80, 32, 119, 178, 37, 108,
      200, 7, 131, 127, 58, 172, 209, 24, 235, 75, 156, 43, 174, 184, 151, 6,
      134, 37, 171, 172, 161, 147,
    ]),
  };

  if (includeLong) {
    sourceData.long = new Uint8Array(1024 * sourceData.medium.byteLength);
    for (var i = 0; i < 1024; i++) {
      sourceData.long.set(sourceData.medium, i * sourceData.medium.byteLength);
    }
  }

  return sourceData;
}
