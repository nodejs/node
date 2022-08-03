'use strict';

module.exports = function() {
  const pkcs8 = {
    'Ed25519': Buffer.from(
      '302e020100300506032b657004220420f3c8f4c48df878146e8cd3bf6df4e50e389b' +
      'a7074e15c2352dcd5d308d4ca81f', 'hex'),
    'Ed448': Buffer.from(
      '3047020100300506032b6571043b04390eff03458c28e0179c521de312c969b78343' +
      '48ecab991a60e3b2e9a79e4cd9e480ef291712d2c83d047272d5c9f428664f696d26' +
      '70458f1d2e', 'hex')
  }

  const spki = {
    'Ed25519': Buffer.from(
      '302a300506032b6570032100d8e18963d809d487d9549accaec6742e7eeba24d8a0d' +
      '3b14b7e3caea06893dcc', 'hex'),
    'Ed448': Buffer.from(
      '3043300506032b6571033a00ab4bb885fd7d2c5af24e83710cffa0c74a57e274801d' +
      'b2057b0bdc5ea032b6fe6bc78b8045365aeb26e86e1f14fd349d07c48495f5a46a5a' +
      '80', 'hex')
  }

  const data = Buffer.from(
    '2b7ed0bc7795694ab4acd35903fe8cd7d80f6a1c8688a6c3414409457514a1457855bb' +
    'b219e30a1beea8fe869082d99fc8282f9050d024e59eaf0730ba9db70a', 'hex');

  // For verification tests.
  const signatures = {
    'Ed25519': Buffer.from(
      '3d90de5e5743dfc28225bfadb341b116cbf8a3f1ceedbf4adc350ef5d3471843a418' +
      '614dcb6e614862614cf7af1496f9340b3c844ea4dceab1d3d155eb7ecc00', 'hex'),
    'Ed448': Buffer.from(
      '76897e8c50ac6b1132735c09c55f506c0149d2677c75664f8bc10b826fbd9df0a03c' +
      'd986bce8339e64c7d1720ea9361784dc73837765ac2980c0dac0814a8bc187d1c9c9' +
      '07c5dcc07956f85b70930fe42de764177217cb2d52bab7c1debe0ca89ccecbcd63f7' +
      '025a2a5a572b9d23b0642f00', 'hex')
  }

  const algorithms = ['Ed25519', 'Ed448'];

  const vectors = algorithms.map((algorithm) => ({
    publicKeyBuffer: spki[algorithm],
    privateKeyBuffer: pkcs8[algorithm],
    name: algorithm,
    data,
    signature: signatures[algorithm],
  }));

  return vectors;
}
