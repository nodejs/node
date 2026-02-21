'use strict';
// Throughput benchmark in signing and verifying
const common = require('../common.js');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const fixtures_keydir = path.resolve(__dirname, '../../test/fixtures/keys/');
const keylen_list = ['2048'];
const RSA_PublicPem = {};
const RSA_PrivatePem = {};

keylen_list.forEach((key) => {
  RSA_PublicPem[key] =
    fs.readFileSync(`${fixtures_keydir}/rsa_public_${key}.pem`);
  RSA_PrivatePem[key] =
    fs.readFileSync(`${fixtures_keydir}/rsa_private_${key}.pem`);
});

const bench = common.createBenchmark(main, {
  writes: [500],
  algo: ['SHA1', 'SHA224', 'SHA256', 'SHA384', 'SHA512'],
  keylen: keylen_list,
  len: [1024, 102400, 2 * 102400, 3 * 102400, 1024 * 1024],
});

function main({ len, algo, keylen, writes }) {
  const message = Buffer.alloc(len, 'b');
  bench.start();
  StreamWrite(algo, keylen, message, writes, len);
}

function StreamWrite(algo, keylen, message, writes, len) {
  const written = writes * len;
  const bits = written * 8;
  const kbits = bits / (1024);

  const privateKey = RSA_PrivatePem[keylen];
  const s = crypto.createSign(algo);
  const v = crypto.createVerify(algo);

  while (writes-- > 0) {
    s.update(message);
    v.update(message);
  }

  s.sign(privateKey, 'binary');
  s.end();
  v.end();

  bench.end(kbits);
}
