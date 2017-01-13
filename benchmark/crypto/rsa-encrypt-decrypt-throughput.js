'use strict';
// throughput benchmark in signing and verifying
var common = require('../common.js');
var crypto = require('crypto');
var fs = require('fs');
var path = require('path');
var fixtures_keydir = path.resolve(__dirname, '../../test/fixtures/keys/');
var keylen_list = ['1024', '2048', '4096'];
var RSA_PublicPem = {};
var RSA_PrivatePem = {};

keylen_list.forEach(function(key) {
  RSA_PublicPem[key] = fs.readFileSync(fixtures_keydir +
                                       '/rsa_public_' + key + '.pem');
  RSA_PrivatePem[key] = fs.readFileSync(fixtures_keydir +
                                        '/rsa_private_' + key + '.pem');
});

var bench = common.createBenchmark(main, {
  n: [500],
  keylen: keylen_list,
  len: [16, 32, 64]
});

function main(conf) {
  var message = Buffer.alloc(conf.len, 'b');
  bench.start();
  StreamWrite(conf.algo, conf.keylen, message, conf.n, conf.len);
}

function StreamWrite(algo, keylen, message, n, len) {
  var written = n * len;
  var bits = written * 8;
  var kbits = bits / (1024);

  var privateKey = RSA_PrivatePem[keylen];
  var publicKey = RSA_PublicPem[keylen];
  for (var i = 0; i < n; i++) {
    var enc = crypto.privateEncrypt(privateKey, message);
    crypto.publicDecrypt(publicKey, enc);
  }

  bench.end(kbits);
}
