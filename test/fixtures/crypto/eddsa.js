'use strict';

const common = require('../../common');

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
  // eslint-disable @stylistic/js/max-len
  const signatures = {
    'Ed25519': {
      // Ed25519 does not support context
      '0': Buffer.from('3d90de5e5743dfc28225bfadb341b116cbf8a3f1ceedbf4adc350ef5d3471843a418614dcb6e614862614cf7af1496f9340b3c844ea4dceab1d3d155eb7ecc00', 'hex'),
    },
    'Ed448': {
      '0': Buffer.from('76897e8c50ac6b1132735c09c55f506c0149d2677c75664f8bc10b826fbd9df0a03cd986bce8339e64c7d1720ea9361784dc73837765ac2980c0dac0814a8bc187d1c9c907c5dcc07956f85b70930fe42de764177217cb2d52bab7c1debe0ca89ccecbcd63f7025a2a5a572b9d23b0642f00', 'hex'),
      '32': Buffer.from('0294186f0305dd3a2d5ac86eeb7e73c05d419e84152c2341ae24e55c3889e878f4acb537f3651a50b0b1c26739721b168499337537c92727003480be61fc23f519ed772ebf2977f6bda5259235ded904959227beaf0adfbd28288358854e9abe089dc8075998993b86280b0bd89bdacc3c00', 'hex'),
      '255': Buffer.from('6dfef748ab53ca856b3ffd84c62ae167c2737dfe4eae89c6c1edc0adc685b73f8170eacd723ec76fb31318ebe47c908722000129b2e9806e8040a4d4d90ac1d1b539199e33553300dcdf4989e4b77c835b53f4ee0d114845ad97047ad0d112e05304b38f5836bbe024a6f700a368d9910100', 'hex'),
    }
  }
  // eslint-disable @stylistic/js/max-len


  const algorithms = ['Ed25519'];
  const contexts = [new Uint8Array(0), new Uint8Array(32), new Uint8Array(255)];

  if (!process.features.openssl_is_boringssl) {
    algorithms.push('Ed448')
  } else {
    common.printSkipMessage(`Skipping unsupported Ed448 test cases`);
  }

  const vectors = [];
  for (const algorithm of algorithms) {
    for (const context of contexts) {
      if (algorithm === 'Ed25519' && context.byteLength !== 0) {
        continue;
      }

      vectors.push({
        publicKeyBuffer: spki[algorithm],
        privateKeyBuffer: pkcs8[algorithm],
        name: algorithm,
        context: algorithm === 'Ed25519' ? undefined : context,
        data,
        signature: signatures[algorithm][context.byteLength],
      })
    }
  }

  return vectors;
}
