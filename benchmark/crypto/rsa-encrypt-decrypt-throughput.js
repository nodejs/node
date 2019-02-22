'use strict';
// throughput benchmark in signing and verifying
const common = require('../common.js');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const fixtures_keydir = path.resolve(__dirname, '../../test/fixtures/keys/');
const keylen_list = ['1024', '2048', '4096'];
const RSA_PublicPem = {};
const RSA_PrivatePem = {};

keylen_list.forEach((key) => {
  RSA_PublicPem[key] =
    fs.readFileSync(`${fixtures_keydir}/rsa_public_${key}.pem`);
  RSA_PrivatePem[key] =
    fs.readFileSync(`${fixtures_keydir}/rsa_private_${key}.pem`);
});

const bench = common.createBenchmark(main, {
  n: [500],
  keylen: keylen_list,
  len: [16, 32, 64]
});

function main({ len, algo, keylen, n }) {
  const message = Buffer.alloc(len, 'b');
  bench.start();
  StreamWrite(algo, keylen, message, n, len);
}

function StreamWrite(algo, keylen, message, n, len) {
  const written = n * len;
  const bits = written * 8;
  const kbits = bits / (1024);

  const privateKey = RSA_PrivatePem[keylen];
  const publicKey = RSA_PublicPem[keylen];
  for (var i = 0; i < n; i++) {
    const enc = crypto.privateEncrypt(privateKey, message);
    crypto.publicDecrypt(publicKey, enc);
  }

  bench.end(kbits);
}
