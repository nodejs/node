'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  writes: [500],
  cipher: [ 'AES192', 'AES256' ],
  type: ['asc', 'utf', 'buf'],
  len: [2, 1024, 102400, 1024 * 1024],
  api: ['legacy', 'stream']
});

function main({ api, cipher, type, len, writes }) {
  // Default cipher for tests.
  if (cipher === '')
    cipher = 'AES192';
  if (api === 'stream' && /^v0\.[0-8]\./.test(process.version)) {
    console.error('Crypto streams not available until v0.10');
    // use the legacy, just so that we can compare them.
    api = 'legacy';
  }

  const crypto = require('crypto');
  const assert = require('assert');
  const alice = crypto.getDiffieHellman('modp5');
  const bob = crypto.getDiffieHellman('modp5');

  alice.generateKeys();
  bob.generateKeys();


  const pubEnc = /^v0\.[0-8]/.test(process.version) ? 'binary' : null;
  const alice_secret = alice.computeSecret(bob.getPublicKey(), pubEnc, 'hex');
  const bob_secret = bob.computeSecret(alice.getPublicKey(), pubEnc, 'hex');

  // alice_secret and bob_secret should be the same
  assert(alice_secret === bob_secret);

  const alice_cipher = crypto.createCipher(cipher, alice_secret);
  const bob_cipher = crypto.createDecipher(cipher, bob_secret);

  var message;
  var encoding;
  switch (type) {
    case 'asc':
      message = 'a'.repeat(len);
      encoding = 'ascii';
      break;
    case 'utf':
      message = 'ü'.repeat(len / 2);
      encoding = 'utf8';
      break;
    case 'buf':
      message = Buffer.alloc(len, 'b');
      break;
    default:
      throw new Error(`unknown message type: ${type}`);
  }

  const fn = api === 'stream' ? streamWrite : legacyWrite;

  // Write data as fast as possible to alice, and have bob decrypt.
  // use old API for comparison to v0.8
  bench.start();
  fn(alice_cipher, bob_cipher, message, encoding, writes);
}

function streamWrite(alice, bob, message, encoding, writes) {
  var written = 0;
  bob.on('data', (c) => {
    written += c.length;
  });

  bob.on('end', () => {
    // Gbits
    const bits = written * 8;
    const gbits = bits / (1024 * 1024 * 1024);
    bench.end(gbits);
  });

  alice.pipe(bob);

  while (writes-- > 0)
    alice.write(message, encoding);

  alice.end();
}

function legacyWrite(alice, bob, message, encoding, writes) {
  var written = 0;
  var enc, dec;
  for (var i = 0; i < writes; i++) {
    enc = alice.update(message, encoding);
    dec = bob.update(enc);
    written += dec.length;
  }
  enc = alice.final();
  dec = bob.update(enc);
  written += dec.length;
  dec = bob.final();
  written += dec.length;
  const gbits = written / (1024 * 1024 * 1024);
  bench.end(gbits);
}
