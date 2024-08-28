'use strict';

module.exports = function () {
  const plaintext = Buffer.from(
    '5f4dba4f320c0ce876725afce5fbd25bf83e5a7125a08cafe73c3ebac421779df9d55d' +
    '180c3ae9942645e1d82fee8c9d294b3cb1a08a9931201b3c0e81fc47cacf8315a2af66' +
    '324113c3b66230c34608c4f4593634ce02b267362277f0a840ca74bc3d1a6236952c5e' +
    'd7aaf8a8fecbddfa7584e6978cea5d2a5b9fb7f1b48c8b0be58a305202754d83761073' +
    '74793cf026aaee5300727d836cd71e71b345ddb2e44446ffc5b901635413890d910ea3' +
    '80984a90191031323f16dbcc9d6be168b84885384ca03e12600ac1c248028af3726cc9' +
    '3463882ea8c02aab', 'hex');

  const raw = {
    'SHA-1': Buffer.from('47a20746d17179db65e0a79dedffc7fdf181081b', 'hex'),
    'SHA-256': Buffer.from(
      'e588ec0811463d767241df1074b47ae4071b51f2ce36537ba69ccdc3fdc2b7a8',
      'hex'),
    'SHA-384': Buffer.from(
      '6b1da28eab1f582ad9718effe05e23d5fd2c9877a2d9443f90bec093bece2ea7' +
      'd2354cd0bdc5e147d2e9009373494488', 'hex'),
    'SHA-512': Buffer.from(
      '5dcc359443aaf652fa1375d6b3e61fdcf29bb4a28bd5d3dcfa40f82f906bb280' +
      '0455db03b5d31fb972a15a6d0103a24e56d156a119c0e5a1e92a44c3c5657cf9',
      'hex'),
    'SHA3-256': Buffer.from(
      'e588ec0811463d767241df1074b47ae4071b51f2ce36537ba69ccdc3fdc2b7a8',
      'hex'),
    'SHA3-384': Buffer.from(
      '6b1da28eab1f582ad9718effe05e23d5fd2c9877a2d9443f90bec093bece2ea7' +
      'd2354cd0bdc5e147d2e9009373494488', 'hex'),
    'SHA3-512': Buffer.from(
      '5dcc359443aaf652fa1375d6b3e61fdcf29bb4a28bd5d3dcfa40f82f906bb280' +
      '0455db03b5d31fb972a15a6d0103a24e56d156a119c0e5a1e92a44c3c5657cf9',
      'hex')
  }

  const signatures = {
    'SHA-1': Buffer.from('0533902a99f8524ee50af01d38dedce133d98ca0', 'hex'),
    'SHA-256': Buffer.from(
      '85a40cea2e078c2827a3953ffb66c27b291a472b0d70a0000b45d823803eeb54',
      'hex'),
    'SHA-384': Buffer.from(
      '217c3d50f0ba9a6d6eae1efdd7a518fe2e3880b582a40d061e9099c1e026ef58' +
      '82548b5d5cecdd5598d99b6b6f3057ff', 'hex'),
    'SHA-512': Buffer.from(
      '61fb278c3ffb0cce2bf1cf723ddfd8ef1f931c0c618c25907324605939e3f9a2' +
      'c6f4af690bda3407dc2f5770f6a0a44b954d64a332e3ee0821abf82b7f3e99c1',
      'hex'),
    'SHA3-256': Buffer.from(
      'c1ac5e11fcd50c48bf567f6e296632f5801c4eb07a8a47579b41dee971a3099b',
      'hex'),
    'SHA3-384': Buffer.from(
      'ac8c97f6dd8d9e16101063077c16b23fe291a5e6d149653e9ac7002365159317' +
      'adcfad511996578b0053a5c14b75f16c', 'hex'),
    'SHA3-512': Buffer.from(
      '2162c2a8907e6b2f68599a69e81a464d8f076b5eeb555d98b4d20330034df3c7' +
      'cf35b1fa958a074ca12f0d242df39f0da3d4f1dbfb3629057798fe1f883974ee',
      'hex')
  }

  const vectors = [];
  Object.keys(raw).forEach((hash) => {
    vectors.push({
      hash,
      keyBuffer: raw[hash],
      plaintext,
      signature: signatures[hash]
    });
  });

  return vectors;
};
