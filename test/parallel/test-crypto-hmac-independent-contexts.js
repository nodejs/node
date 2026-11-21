'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

// HMAC instances for the same digest start from one cached context. Live
// instances with different keys must not affect each other.

// Test cases 1, 2 and 6 from RFC 4231. Case 6 uses a key longer than the block
// size, which is hashed first.
const vectors = [
  {
    key: Buffer.alloc(20, 0x0b),
    data: 'Hi There',
    sha256: 'b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7',
    sha512: '87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cde' +
            'daa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854',
  },
  {
    key: Buffer.from('Jefe'),
    data: 'what do ya want for nothing?',
    sha256: '5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843',
    sha512: '164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea250554' +
            '9758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737',
  },
  {
    key: Buffer.alloc(131, 0xaa),
    data: 'Test Using Larger Than Block-Size Key - Hash Key First',
    sha256: '60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54',
    sha512: '80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f352' +
            '6b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598',
  },
];

function chunks(data, size) {
  const result = [];
  for (let i = 0; i < data.length; i += size)
    result.push(data.slice(i, i + size));
  return result;
}

for (const hash of ['sha256', 'sha512']) {
  // Create every instance before updating any of them.
  const hmacs = vectors.map(({ key }) => crypto.createHmac(hash, key));
  const parts = vectors.map(({ data }) => chunks(data, 5));

  // Interleave the updates across instances.
  const rounds = Math.max(...parts.map((p) => p.length));
  for (let round = 0; round < rounds; round++) {
    for (let i = 0; i < hmacs.length; i++) {
      if (round < parts[i].length) hmacs[i].update(parts[i][round]);
    }
  }

  // Finish them in reverse order.
  for (let i = hmacs.length - 1; i >= 0; i--)
    assert.strictEqual(hmacs[i].digest('hex'), vectors[i][hash]);

  // Creating and finishing another instance does not affect one that is in
  // progress, and later instances start from a clean context.
  const { key, data } = vectors[1];
  const early = crypto.createHmac(hash, key).update(data.slice(0, 10));
  const other = crypto.createHmac(hash, vectors[0].key);
  assert.strictEqual(other.update(vectors[0].data).digest('hex'),
                     vectors[0][hash]);
  assert.strictEqual(early.update(data.slice(10)).digest('hex'),
                     vectors[1][hash]);
  assert.strictEqual(crypto.createHmac(hash, key).update(data).digest('hex'),
                     vectors[1][hash]);
}
