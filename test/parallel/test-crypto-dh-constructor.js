'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { hasFIPS } = require('../common/crypto');

const prime = crypto.getDiffieHellman('modp14').getPrime('buffer');

{
  const DiffieHellman = crypto.DiffieHellman;

  const dh = DiffieHellman(prime, 'buffer');
  assert(dh instanceof DiffieHellman, 'DiffieHellman is expected to return a ' +
                                      'new instance when called without `new`');
}

{
  const DiffieHellmanGroup = crypto.DiffieHellmanGroup;
  const dhg = DiffieHellmanGroup(hasFIPS(3) ? 'modp14' : 'modp5');
  assert(dhg instanceof DiffieHellmanGroup, 'DiffieHellmanGroup is expected ' +
                                            'to return a new instance when ' +
                                            'called without `new`');
}

{
  const ECDH = crypto.ECDH;
  const ecdh = ECDH('prime256v1');
  assert(ecdh instanceof ECDH, 'ECDH is expected to return a new instance ' +
                              'when called without `new`');
}
