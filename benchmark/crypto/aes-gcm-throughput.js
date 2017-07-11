'use strict';
var common = require('../common.js');
var crypto = require('crypto');
var keylen = { 'aes-128-gcm': 16, 'aes-192-gcm': 24, 'aes-256-gcm': 32 };
var bench = common.createBenchmark(main, {
  n: [500],
  cipher: ['aes-128-gcm', 'aes-192-gcm', 'aes-256-gcm'],
  len: [1024, 4 * 1024, 16 * 1024, 64 * 1024, 256 * 1024, 1024 * 1024]
});

function main(conf) {
  var message = Buffer.alloc(conf.len, 'b');
  var key = crypto.randomBytes(keylen[conf.cipher]);
  var iv = crypto.randomBytes(12);
  var associate_data = Buffer.alloc(16, 'z');
  bench.start();
  AEAD_Bench(conf.cipher, message, associate_data, key, iv, conf.n, conf.len);
}

function AEAD_Bench(cipher, message, associate_data, key, iv, n, len) {
  var written = n * len;
  var bits = written * 8;
  var mbits = bits / (1024 * 1024);

  for (var i = 0; i < n; i++) {
    var alice = crypto.createCipheriv(cipher, key, iv);
    alice.setAAD(associate_data);
    var enc = alice.update(message);
    alice.final();
    var tag = alice.getAuthTag();
    var bob = crypto.createDecipheriv(cipher, key, iv);
    bob.setAuthTag(tag);
    bob.setAAD(associate_data);
    bob.update(enc);
    bob.final();
  }

  bench.end(mbits);
}
