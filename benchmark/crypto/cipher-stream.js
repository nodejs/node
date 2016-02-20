'use strict';
var common = require('../common.js');

var bench = common.createBenchmark(main, {
  writes: [500],
  cipher: [ 'AES192', 'AES256' ],
  type: ['asc', 'utf', 'buf'],
  len: [2, 1024, 102400, 1024 * 1024],
  api: ['legacy', 'stream']
});

function main(conf) {
  var api = conf.api;
  if (api === 'stream' && process.version.match(/^v0\.[0-8]\./)) {
    console.error('Crypto streams not available until v0.10');
    // use the legacy, just so that we can compare them.
    api = 'legacy';
  }

  var crypto = require('crypto');
  var assert = require('assert');
  var alice = crypto.getDiffieHellman('modp5');
  var bob = crypto.getDiffieHellman('modp5');

  alice.generateKeys();
  bob.generateKeys();


  var pubEnc = /^v0\.[0-8]/.test(process.version) ? 'binary' : null;
  var alice_secret = alice.computeSecret(bob.getPublicKey(), pubEnc, 'hex');
  var bob_secret = bob.computeSecret(alice.getPublicKey(), pubEnc, 'hex');

  // alice_secret and bob_secret should be the same
  assert(alice_secret == bob_secret);

  var alice_cipher = crypto.createCipher(conf.cipher, alice_secret);
  var bob_cipher = crypto.createDecipher(conf.cipher, bob_secret);

  var message;
  var encoding;
  switch (conf.type) {
    case 'asc':
      message = new Array(conf.len + 1).join('a');
      encoding = 'ascii';
      break;
    case 'utf':
      message = new Array(conf.len / 2 + 1).join('ü');
      encoding = 'utf8';
      break;
    case 'buf':
      message = new Buffer(conf.len);
      message.fill('b');
      break;
    default:
      throw new Error('unknown message type: ' + conf.type);
  }

  var fn = api === 'stream' ? streamWrite : legacyWrite;

  // write data as fast as possible to alice, and have bob decrypt.
  // use old API for comparison to v0.8
  bench.start();
  fn(alice_cipher, bob_cipher, message, encoding, conf.writes);
}

function streamWrite(alice, bob, message, encoding, writes) {
  var written = 0;
  bob.on('data', function(c) {
    written += c.length;
  });

  bob.on('end', function() {
    // Gbits
    var bits = written * 8;
    var gbits = bits / (1024 * 1024 * 1024);
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
  var gbits = written / (1024 * 1024 * 1024);
  bench.end(gbits);
}
