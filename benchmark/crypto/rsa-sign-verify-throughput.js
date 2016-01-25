'use strict';
// throughput benchmark in signing and verifying
var common = require('../common.js');
var crypto = require('crypto');
var fs = require('fs');
var path = require('path');
var fixtures_keydir = path.resolve(__dirname, '../../test/fixtures/keys/');
var keylen_list = ['1024', '2048'];
var RSA_PublicPem = {};
var RSA_PrivatePem = {};

keylen_list.forEach(function(key) {
  RSA_PublicPem[key] = fs.readFileSync(fixtures_keydir +
                                       '/rsa_public_' + key + '.pem');
  RSA_PrivatePem[key] = fs.readFileSync(fixtures_keydir +
                                        '/rsa_private_' + key + '.pem');
});

var bench = common.createBenchmark(main, {
  writes: [500],
  algo: ['RSA-SHA1', 'RSA-SHA224', 'RSA-SHA256', 'RSA-SHA384', 'RSA-SHA512'],
  keylen: keylen_list,
  len: [1024, 102400, 2 * 102400, 3 * 102400, 1024 * 1024]
});

function main(conf) {
  var message = Buffer.alloc(conf.len, 'b');
  bench.start();
  StreamWrite(conf.algo, conf.keylen, message, conf.writes, conf.len);
}

function StreamWrite(algo, keylen, message, writes, len) {
  var written = writes * len;
  var bits = written * 8;
  var kbits = bits / (1024);

  var privateKey = RSA_PrivatePem[keylen];
  var s = crypto.createSign(algo);
  var v = crypto.createVerify(algo);

  while (writes-- > 0) {
    s.update(message);
    v.update(message);
  }

  s.sign(privateKey, 'binary');
  s.end();
  v.end();

  bench.end(kbits);
}
