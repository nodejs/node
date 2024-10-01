'use strict';

module.exports = function() {
  const pkcs8 = {
    'Ed25519': Buffer.from(
      '302e020100300506032b657004220420f3c8f4c48df878146e8cd3bf6df4e50e389b' +
      'a7074e15c2352dcd5d308d4ca81f', 'hex'),
  }

  const spki = {
    'Ed25519': Buffer.from(
      '302a300506032b6570032100d8e18963d809d487d9549accaec6742e7eeba24d8a0d' +
      '3b14b7e3caea06893dcc', 'hex'),
  }

  const data = Buffer.from(
    '2b7ed0bc7795694ab4acd35903fe8cd7d80f6a1c8688a6c3414409457514a1457855bb' +
    'b219e30a1beea8fe869082d99fc8282f9050d024e59eaf0730ba9db70a', 'hex');

  // For verification tests.
  const signatures = {
    'Ed25519': Buffer.from(
      '3d90de5e5743dfc28225bfadb341b116cbf8a3f1ceedbf4adc350ef5d3471843a418' +
      '614dcb6e614862614cf7af1496f9340b3c844ea4dceab1d3d155eb7ecc00', 'hex'),
  }

  const algorithms = ['Ed25519'];

  const vectors = algorithms.map((algorithm) => ({
    publicKeyBuffer: spki[algorithm],
    privateKeyBuffer: pkcs8[algorithm],
    name: algorithm,
    data,
    signature: signatures[algorithm],
  }));

  return vectors;
}
